#include "VMEffectsSubsystem.h"
#include "VMUtil.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/World.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

namespace
{
	struct FLayerStyle
	{
		const TCHAR* Shape;
		FLinearColor Color;
	};

	FLayerStyle GetStyle(EVMParticleLayer Layer)
	{
		switch (Layer)
		{
		case EVMParticleLayer::Spark:      return { TEXT("Cube"),   FLinearColor(1.f, 0.78f, 0.25f) };
		case EVMParticleLayer::Fire:       return { TEXT("Sphere"), FLinearColor(1.f, 0.35f, 0.03f) };
		case EVMParticleLayer::SmokeLight: return { TEXT("Sphere"), FLinearColor(0.55f, 0.55f, 0.57f) };
		case EVMParticleLayer::SmokeDark:  return { TEXT("Sphere"), FLinearColor(0.05f, 0.05f, 0.05f) };
		case EVMParticleLayer::Blood:      return { TEXT("Cube"),   FLinearColor(0.35f, 0.f, 0.f) };
		case EVMParticleLayer::Energy:     return { TEXT("Cube"),   FLinearColor(0.75f, 0.25f, 1.f) };
		case EVMParticleLayer::Flame:      return { TEXT("Sphere"), FLinearColor(0.35f, 0.65f, 1.f) };
		default:                           return { TEXT("Cube"),   FLinearColor::White };
		}
	}
}

void UVMEffectsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Layers.SetNum(static_cast<int32>(EVMParticleLayer::Count));
	Rng.GenerateNewSeed();
}

TStatId UVMEffectsSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UVMEffectsSubsystem, STATGROUP_Tickables);
}

UVMEffectsSubsystem* UVMEffectsSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return (World && World->IsGameWorld()) ? World->GetSubsystem<UVMEffectsSubsystem>() : nullptr;
}

bool UVMEffectsSubsystem::EnsureHost()
{
	if (Host)
	{
		return true;
	}
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return false;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Host = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
	if (!Host)
	{
		return false;
	}
	USceneComponent* Root = NewObject<USceneComponent>(Host, TEXT("Root"));
	Host->SetRootComponent(Root);
	Root->RegisterComponent();

	for (int32 i = 0; i < static_cast<int32>(EVMParticleLayer::Count); ++i)
	{
		const FLayerStyle Style = GetStyle(static_cast<EVMParticleLayer>(i));
		UInstancedStaticMeshComponent* ISM = NewObject<UInstancedStaticMeshComponent>(Host);
		ISM->SetMobility(EComponentMobility::Movable);
		ISM->SetStaticMesh(VMUtil::Shape(Style.Shape));
		ISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ISM->SetCastShadow(false);
		ISM->SetupAttachment(Root);
		ISM->RegisterComponent();
		VMUtil::Paint(ISM, Style.Color);
		LayerComponents.Add(ISM);
	}
	return true;
}

int32 UVMEffectsSubsystem::Scaled(int32 Count) const
{
	return FMath::Max(1, FMath::RoundToInt(Count * GetDefault<UVMEffectsSettings>()->ParticleDensity));
}

void UVMEffectsSubsystem::Emit(EVMParticleLayer Layer, const FVMParticle& Particle)
{
	if (!EnsureHost())
	{
		return;
	}
	TArray<FVMParticle>& List = Layers[static_cast<int32>(Layer)];
	if (List.Num() >= GetDefault<UVMEffectsSettings>()->MaxParticlesPerLayer)
	{
		// Recycle the oldest-ish slot rather than dropping the new particle.
		List[Rng.RandRange(0, List.Num() - 1)] = Particle;
		return;
	}
	List.Add(Particle);
}

bool UVMEffectsSubsystem::SpawnOverride(EVMEffect Effect, const FVector& Location, const FRotator& Rotation)
{
	const UVMEffectsSettings* Settings = GetDefault<UVMEffectsSettings>();
	const TSoftObjectPtr<UNiagaraSystem>* Override = Settings->NiagaraOverrides.Find(Effect);
	if (!Override || Override->IsNull())
	{
		return false;
	}
	if (UNiagaraSystem* System = Override->LoadSynchronous())
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), System, Location, Rotation);
		return true;
	}
	return false;
}

// ---------------------------------------------------------------------------------------------
// Effects
// ---------------------------------------------------------------------------------------------

void UVMEffectsSubsystem::Sparks(const FVector& Location, const FVector& Normal, int32 Count, float Speed)
{
	const FVector N = Normal.IsNearlyZero() ? FVector::UpVector : Normal.GetSafeNormal();
	if (SpawnOverride(EVMEffect::Sparks, Location, N.Rotation()))
	{
		return;
	}
	for (int32 i = 0, Num = Scaled(Count); i < Num; ++i)
	{
		FVMParticle P;
		P.Position = Location;
		// Spray in a cone around the surface normal, biased upward.
		const FVector Dir = (N + Rng.GetUnitVector() * 0.8f + FVector(0.f, 0.f, 0.3f)).GetSafeNormal();
		P.Velocity = Dir * Speed * Rng.FRandRange(0.4f, 1.3f);
		P.Life = Rng.FRandRange(0.25f, 0.7f);
		P.StartSize = 4.f;
		P.EndSize = 2.f;
		P.Gravity = 980.f;
		P.Drag = 1.5f;
		P.bStretch = true;
		Emit(EVMParticleLayer::Spark, P);
	}
}

void UVMEffectsSubsystem::Explosion(const FVector& Location)
{
	if (SpawnOverride(EVMEffect::Explosion, Location, FRotator::ZeroRotator))
	{
		return;
	}
	for (int32 i = 0, Num = Scaled(40); i < Num; ++i)
	{
		FVMParticle P;
		P.Position = Location + Rng.GetUnitVector() * 60.f;
		P.Velocity = (Rng.GetUnitVector() + FVector(0.f, 0.f, 0.6f)).GetSafeNormal() * Rng.FRandRange(300.f, 900.f);
		P.Life = Rng.FRandRange(0.3f, 0.8f);
		P.StartSize = Rng.FRandRange(60.f, 120.f);
		P.EndSize = 20.f;
		P.Drag = 3.f;
		P.Gravity = -200.f;
		Emit(EVMParticleLayer::Fire, P);
	}
	for (int32 i = 0, Num = Scaled(30); i < Num; ++i)
	{
		FVMParticle P;
		P.Position = Location + Rng.GetUnitVector() * 100.f;
		P.Velocity = (Rng.GetUnitVector() * 0.5f + FVector(0.f, 0.f, 1.f)) * Rng.FRandRange(200.f, 500.f);
		P.Life = Rng.FRandRange(1.5f, 3.f);
		P.StartSize = Rng.FRandRange(60.f, 100.f);
		P.EndSize = Rng.FRandRange(200.f, 320.f);
		P.Drag = 1.2f;
		P.Gravity = -60.f;
		Emit(EVMParticleLayer::SmokeDark, P);
	}
	Sparks(Location, FVector::UpVector, 50, 1600.f);
}

void UVMEffectsSubsystem::Splat(const FVector& Location, const FVector& ImpactVelocity)
{
	if (SpawnOverride(EVMEffect::Splat, Location, ImpactVelocity.Rotation()))
	{
		return;
	}
	for (int32 i = 0, Num = Scaled(24); i < Num; ++i)
	{
		FVMParticle P;
		P.Position = Location + FVector(0.f, 0.f, 60.f);
		P.Velocity = ImpactVelocity * Rng.FRandRange(0.2f, 0.6f) + (Rng.GetUnitVector() + FVector(0.f, 0.f, 0.8f)) * Rng.FRandRange(150.f, 450.f);
		P.Rotation = FRotator(Rng.FRandRange(0.f, 360.f), Rng.FRandRange(0.f, 360.f), 0.f);
		P.Life = Rng.FRandRange(0.5f, 1.1f);
		P.StartSize = Rng.FRandRange(6.f, 14.f);
		P.EndSize = 4.f;
		P.Gravity = 980.f;
		P.Drag = 0.8f;
		Emit(EVMParticleLayer::Blood, P);
	}
}

void UVMEffectsSubsystem::EMPBurst(const FVector& Location, float Radius)
{
	if (SpawnOverride(EVMEffect::EMP, Location, FRotator::ZeroRotator))
	{
		return;
	}
	// An expanding ring that reaches the EMP radius as it fades.
	const int32 Num = Scaled(72);
	const float Life = 0.5f;
	for (int32 i = 0; i < Num; ++i)
	{
		const float Angle = 2.f * PI * i / Num;
		const FVector Dir(FMath::Cos(Angle), FMath::Sin(Angle), 0.f);
		FVMParticle P;
		P.Position = Location + FVector(0.f, 0.f, 80.f);
		P.Velocity = Dir * (Radius / Life);
		P.Life = Life;
		P.StartSize = 40.f;
		P.EndSize = 15.f;
		P.bStretch = true;
		Emit(EVMParticleLayer::Energy, P);
	}
}

void UVMEffectsSubsystem::SmokePuff(const FVector& Location, const FVector& Velocity, bool bDark, float Size)
{
	FVMParticle P;
	P.Position = Location + Rng.GetUnitVector() * 15.f;
	P.Velocity = Velocity + Rng.GetUnitVector() * 40.f;
	P.Life = Rng.FRandRange(1.2f, 2.2f);
	P.StartSize = Size * 0.4f;
	P.EndSize = Size * Rng.FRandRange(1.4f, 2.f);
	P.Drag = 1.5f;
	P.Gravity = -80.f; // smoke rises
	Emit(bDark ? EVMParticleLayer::SmokeDark : EVMParticleLayer::SmokeLight, P);
}

void UVMEffectsSubsystem::TyreSmoke(const FVector& Location, const FVector& CarVelocity)
{
	FVMParticle P;
	P.Position = Location + Rng.GetUnitVector() * 20.f;
	P.Velocity = CarVelocity * 0.1f + Rng.GetUnitVector() * 60.f + FVector(0.f, 0.f, 40.f);
	P.Life = Rng.FRandRange(0.8f, 1.6f);
	P.StartSize = 30.f;
	P.EndSize = Rng.FRandRange(120.f, 180.f);
	P.Drag = 2.5f;
	P.Gravity = -30.f;
	Emit(EVMParticleLayer::SmokeLight, P);
}

void UVMEffectsSubsystem::FirePuff(const FVector& Location)
{
	FVMParticle P;
	P.Position = Location + Rng.GetUnitVector() * 25.f;
	P.Velocity = FVector(0.f, 0.f, Rng.FRandRange(150.f, 300.f)) + Rng.GetUnitVector() * 40.f;
	P.Life = Rng.FRandRange(0.3f, 0.6f);
	P.StartSize = Rng.FRandRange(30.f, 55.f);
	P.EndSize = 8.f;
	P.Drag = 1.f;
	Emit(EVMParticleLayer::Fire, P);
}

void UVMEffectsSubsystem::NitroFlame(const FVector& Location, const FVector& Direction, const FVector& CarVelocity)
{
	FVMParticle P;
	P.Position = Location;
	P.Velocity = CarVelocity + Direction.GetSafeNormal() * Rng.FRandRange(500.f, 800.f) + Rng.GetUnitVector() * 40.f;
	P.Life = Rng.FRandRange(0.08f, 0.18f);
	P.StartSize = 22.f;
	P.EndSize = 6.f;
	P.Drag = 4.f;
	Emit(Rng.FRand() < 0.7f ? EVMParticleLayer::Flame : EVMParticleLayer::Fire, P);
}

void UVMEffectsSubsystem::OilSplash(const FVector& Location)
{
	for (int32 i = 0, Num = Scaled(18); i < Num; ++i)
	{
		FVMParticle P;
		P.Position = Location + FVector(0.f, 0.f, 20.f);
		P.Velocity = (Rng.GetUnitVector() + FVector(0.f, 0.f, 1.2f)).GetSafeNormal() * Rng.FRandRange(150.f, 400.f);
		P.Life = Rng.FRandRange(0.4f, 0.8f);
		P.StartSize = Rng.FRandRange(8.f, 16.f);
		P.EndSize = 5.f;
		P.Gravity = 980.f;
		Emit(EVMParticleLayer::SmokeDark, P);
	}
}

// ---------------------------------------------------------------------------------------------
// Simulation
// ---------------------------------------------------------------------------------------------

void UVMEffectsSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!Host)
	{
		return;
	}

	for (int32 LayerIndex = 0; LayerIndex < Layers.Num(); ++LayerIndex)
	{
		TArray<FVMParticle>& List = Layers[LayerIndex];
		UInstancedStaticMeshComponent* ISM = LayerComponents.IsValidIndex(LayerIndex) ? LayerComponents[LayerIndex].Get() : nullptr;
		if (!ISM)
		{
			continue;
		}

		Scratch.Reset(List.Num());
		for (int32 i = List.Num() - 1; i >= 0; --i)
		{
			FVMParticle& P = List[i];
			P.Age += DeltaTime;
			if (P.Age >= P.Life)
			{
				List.RemoveAtSwap(i, 1, EAllowShrinking::No);
				continue;
			}
			P.Velocity.Z -= P.Gravity * DeltaTime;
			P.Velocity *= FMath::Exp(-P.Drag * DeltaTime);
			P.Position += P.Velocity * DeltaTime;
		}

		for (const FVMParticle& P : List)
		{
			const float T = P.Age / P.Life;
			float Size = FMath::Lerp(P.StartSize, P.EndSize, T);
			// Materials are opaque, so particles shrink away instead of fading.
			if (T > 0.7f)
			{
				Size *= (1.f - T) / 0.3f;
			}
			const float S = FMath::Max(Size, 0.1f) / 100.f;
			if (P.bStretch)
			{
				const float Length = FMath::Clamp(P.Velocity.Size() * 0.03f, Size, 120.f) / 100.f;
				Scratch.Emplace(P.Velocity.Rotation(), P.Position, FVector(Length, S, S));
			}
			else
			{
				Scratch.Emplace(P.Rotation, P.Position, FVector(S));
			}
		}

		if (ISM->GetInstanceCount() == Scratch.Num())
		{
			if (Scratch.Num() > 0)
			{
				ISM->BatchUpdateInstancesTransforms(0, Scratch, /*bWorldSpace*/ true, /*bMarkRenderStateDirty*/ true, /*bTeleport*/ true);
			}
		}
		else
		{
			ISM->ClearInstances();
			if (Scratch.Num() > 0)
			{
				ISM->AddInstances(Scratch, /*bShouldReturnIndices*/ false, /*bWorldSpace*/ true);
			}
		}
	}
}
