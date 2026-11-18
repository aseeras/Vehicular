#include "VMArenaBuilder.h"
#include "VMCheckpoint.h"
#include "VMPickup.h"
#include "VMUtil.h"
#include "Engine/CollisionProfile.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/World.h"
#include "EngineUtils.h"

namespace ArenaColors
{
	static const FLinearColor Ground(0.04f, 0.04f, 0.045f);
	static const FLinearColor Track(0.09f, 0.085f, 0.08f);
	static const FLinearColor Wall(0.35f, 0.33f, 0.3f);
	static const FLinearColor BarrierA(0.7f, 0.05f, 0.03f);
	static const FLinearColor BarrierB(0.85f, 0.85f, 0.85f);
	static const FLinearColor Ramp(0.95f, 0.65f, 0.f);
	static const FLinearColor Pillar(0.15f, 0.3f, 0.45f);
}

AVMArenaBuilder::AVMArenaBuilder()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	CubeMesh = VMUtil::Shape(TEXT("Cube"));
	CylinderMesh = VMUtil::Shape(TEXT("Cylinder"));
}

UStaticMeshComponent* AVMArenaBuilder::AddShape(UStaticMesh* Mesh, const FTransform& Transform, const FLinearColor& Color, bool bCollide)
{
	UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(this);
	Comp->SetMobility(EComponentMobility::Movable);
	Comp->SetStaticMesh(Mesh);
	Comp->SetupAttachment(Root);
	Comp->SetWorldTransform(Transform);
	if (bCollide)
	{
		Comp->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	}
	else
	{
		Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Comp->SetCastShadow(false);
	}
	Comp->RegisterComponent();
	AddInstanceComponent(Comp);
	VMUtil::Paint(Comp, Color);
	return Comp;
}

void AVMArenaBuilder::AddRamp(const FVector& Base, float Yaw, float Length, float Width, float PitchDeg, const FLinearColor& Color)
{
	const FRotator Rot(PitchDeg, Yaw, 0.f);
	const float Thickness = 40.f;
	const FVector Up = FRotationMatrix(Rot).GetUnitAxis(EAxis::Z);
	// Put the low edge of the top surface at Base so cars roll straight onto it.
	const FVector Center = Base + Rot.Vector() * (Length * 0.5f) - Up * (Thickness * 0.5f);
	AddShape(CubeMesh, FTransform(Rot, Center, FVector(Length / 100.f, Width / 100.f, Thickness / 100.f)), Color);
}

FVector AVMArenaBuilder::EllipsePoint(float Theta, float Scale) const
{
	return FVector(TrackRadiusX * Scale * FMath::Cos(Theta), TrackRadiusY * Scale * FMath::Sin(Theta), 0.f);
}

FVector AVMArenaBuilder::EllipseTangent(float Theta) const
{
	return FVector(-TrackRadiusX * FMath::Sin(Theta), TrackRadiusY * FMath::Cos(Theta), 0.f).GetSafeNormal();
}

void AVMArenaBuilder::SpawnPickup(const FVector& Location)
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	GetWorld()->SpawnActor<AVMPickup>(Location, FRotator::ZeroRotator, Params);
}

void AVMArenaBuilder::Build()
{
	if (bBuilt)
	{
		return;
	}
	bBuilt = true;
	Rng.Initialize(RandomSeed);

	HalfX = TrackRadiusX + TrackHalfWidth + 2500.f;
	HalfY = TrackRadiusY + TrackHalfWidth + 2500.f;

	// Ground slab, top surface at Z = 0.
	AddShape(CubeMesh, FTransform(FRotator::ZeroRotator, FVector(0.f, 0.f, -50.f), FVector(HalfX / 50.f, HalfY / 50.f, 1.f)), ArenaColors::Ground);

	// Perimeter walls.
	const float WallH = 500.f;
	const float WallT = 200.f;
	AddShape(CubeMesh, FTransform(FRotator::ZeroRotator, FVector(0.f, HalfY + WallT * 0.5f, WallH * 0.5f), FVector(HalfX / 50.f + 4.f, WallT / 100.f, WallH / 100.f)), ArenaColors::Wall);
	AddShape(CubeMesh, FTransform(FRotator::ZeroRotator, FVector(0.f, -HalfY - WallT * 0.5f, WallH * 0.5f), FVector(HalfX / 50.f + 4.f, WallT / 100.f, WallH / 100.f)), ArenaColors::Wall);
	AddShape(CubeMesh, FTransform(FRotator::ZeroRotator, FVector(HalfX + WallT * 0.5f, 0.f, WallH * 0.5f), FVector(WallT / 100.f, HalfY / 50.f + 4.f, WallH / 100.f)), ArenaColors::Wall);
	AddShape(CubeMesh, FTransform(FRotator::ZeroRotator, FVector(-HalfX - WallT * 0.5f, 0.f, WallH * 0.5f), FVector(WallT / 100.f, HalfY / 50.f + 4.f, WallH / 100.f)), ArenaColors::Wall);

	// Painted racing surface (visual only).
	const int32 TrackSegments = 72;
	for (int32 i = 0; i < TrackSegments; ++i)
	{
		const float T0 = 2.f * PI * i / TrackSegments;
		const float T1 = 2.f * PI * (i + 1) / TrackSegments;
		const FVector P0 = EllipsePoint(T0);
		const FVector P1 = EllipsePoint(T1);
		const FVector Mid = (P0 + P1) * 0.5f + FVector(0.f, 0.f, 1.f);
		const float Len = FVector::Dist(P0, P1) + 60.f;
		const float Yaw = (P1 - P0).Rotation().Yaw;
		AddShape(CubeMesh, FTransform(FRotator(0.f, Yaw, 0.f), Mid, FVector(Len / 100.f, TrackHalfWidth * 2.f / 100.f, 0.02f)), ArenaColors::Track, false);
	}

	// Inner barrier with gaps for shortcuts into the plaza.
	const int32 BarrierSegments = 48;
	const float InnerScaleX = (TrackRadiusX - TrackHalfWidth - 100.f) / TrackRadiusX;
	const float InnerScaleY = (TrackRadiusY - TrackHalfWidth - 100.f) / TrackRadiusY;
	for (int32 i = 0; i < BarrierSegments; ++i)
	{
		if (i % 6 == 3)
		{
			continue;
		}
		const float T0 = 2.f * PI * i / BarrierSegments;
		const float T1 = 2.f * PI * (i + 1) / BarrierSegments;
		const FVector P0(TrackRadiusX * InnerScaleX * FMath::Cos(T0), TrackRadiusY * InnerScaleY * FMath::Sin(T0), 0.f);
		const FVector P1(TrackRadiusX * InnerScaleX * FMath::Cos(T1), TrackRadiusY * InnerScaleY * FMath::Sin(T1), 0.f);
		const FVector Mid = (P0 + P1) * 0.5f + FVector(0.f, 0.f, 60.f);
		const float Len = FVector::Dist(P0, P1);
		const float Yaw = (P1 - P0).Rotation().Yaw;
		AddShape(CubeMesh, FTransform(FRotator(0.f, Yaw, 0.f), Mid, FVector(Len / 100.f + 0.1f, 0.8f, 1.2f)), (i % 2) ? ArenaColors::BarrierA : ArenaColors::BarrierB);
	}

	// Plaza: facing ramps make a gap jump through the middle, plus a side kicker.
	AddRamp(FVector(-2800.f, 0.f, 0.f), 0.f, 1600.f, 900.f, 14.f, ArenaColors::Ramp);
	AddRamp(FVector(2800.f, 0.f, 0.f), 180.f, 1600.f, 900.f, 14.f, ArenaColors::Ramp);
	AddRamp(FVector(0.f, -3200.f, 0.f), 90.f, 1200.f, 700.f, 18.f, ArenaColors::Ramp);

	// Plaza pillars to weave through (kept out of the jump lane).
	for (int32 i = 0; i < 10; ++i)
	{
		const float Theta = Rng.FRandRange(0.f, 2.f * PI);
		const float Scale = Rng.FRandRange(0.25f, 0.6f);
		FVector P(TrackRadiusX * InnerScaleX * Scale * FMath::Cos(Theta), TrackRadiusY * InnerScaleY * Scale * FMath::Sin(Theta), 0.f);
		if (FMath::Abs(P.Y) < 900.f)
		{
			P.Y = FMath::Sign(P.Y + 0.1f) * 1400.f;
		}
		AddShape(CylinderMesh, FTransform(FRotator::ZeroRotator, P + FVector(0.f, 0.f, 200.f), FVector(1.8f, 1.8f, 4.f)), ArenaColors::Pillar);
	}

	// Kickers on the long straights, off the racing line.
	AddRamp(EllipsePoint(PI * 0.5f, 1.0f) + FVector(0.f, 1300.f, 0.f) - EllipseTangent(PI * 0.5f) * 600.f, EllipseTangent(PI * 0.5f).Rotation().Yaw, 1000.f, 600.f, 12.f, ArenaColors::Ramp);
	AddRamp(EllipsePoint(PI * 1.5f, 1.0f) - FVector(0.f, 1300.f, 0.f) - EllipseTangent(PI * 1.5f) * 600.f, EllipseTangent(PI * 1.5f).Rotation().Yaw, 1000.f, 600.f, 12.f, ArenaColors::Ramp);

	// Checkpoints, counter-clockwise starting at the east end.
	for (int32 i = 0; i < NumCheckpoints; ++i)
	{
		const float Theta = 2.f * PI * i / NumCheckpoints;
		const FTransform T(FRotator(0.f, EllipseTangent(Theta).Rotation().Yaw, 0.f), EllipsePoint(Theta));
		AVMCheckpoint* CP = GetWorld()->SpawnActorDeferred<AVMCheckpoint>(AVMCheckpoint::StaticClass(), T, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		CP->Index = i;
		CP->GateHalfWidth = TrackHalfWidth;
		CP->FinishSpawning(T);
		Checkpoints.Add(CP);
	}

	// Pickups around the circuit (not on the start grid) and in the plaza.
	for (int32 i = 0; i < NumTrackPickups; ++i)
	{
		const float Theta = Rng.FRandRange(0.35f, 2.f * PI - 0.1f);
		SpawnPickup(EllipsePoint(Theta, Rng.FRandRange(0.88f, 1.12f)));
	}
	for (int32 i = 0; i < NumPlazaPickups; ++i)
	{
		const float Theta = Rng.FRandRange(0.f, 2.f * PI);
		SpawnPickup(EllipsePoint(Theta, Rng.FRandRange(0.2f, 0.5f)));
	}
}

FVector AVMArenaBuilder::GetRandomGroundPoint(FRandomStream& InRng) const
{
	const FVector P(InRng.FRandRange(-HalfX + 600.f, HalfX - 600.f), InRng.FRandRange(-HalfY + 600.f, HalfY - 600.f), 0.f);
	FVector Ground;
	return VMUtil::GroundAt(GetWorld(), P, Ground) ? Ground : P;
}

void AVMArenaBuilder::EnsureLighting(UWorld* World)
{
	for (TActorIterator<ADirectionalLight> It(World); It; ++It)
	{
		return; // The level already has its own lighting.
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	if (ADirectionalLight* Sun = World->SpawnActor<ADirectionalLight>(FVector(0.f, 0.f, 5000.f), FRotator(-38.f, 35.f, 0.f), Params))
	{
		Sun->SetMobility(EComponentMobility::Movable);
		Sun->GetLightComponent()->SetIntensity(8.f);
	}
	World->SpawnActor<ASkyAtmosphere>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
	if (ASkyLight* Sky = World->SpawnActor<ASkyLight>(FVector(0.f, 0.f, 3000.f), FRotator::ZeroRotator, Params))
	{
		USkyLightComponent* SkyComp = Sky->GetLightComponent();
		SkyComp->SetMobility(EComponentMobility::Movable);
		SkyComp->bRealTimeCapture = true;
		SkyComp->RecaptureSky();
	}
	World->SpawnActor<AExponentialHeightFog>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
}
