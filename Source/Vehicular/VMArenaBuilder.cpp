#include "VMArenaBuilder.h"
#include "VMCheckpoint.h"
#include "VMPickup.h"
#include "VMUtil.h"
#include "Engine/CollisionProfile.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/World.h"
#include "EngineUtils.h"

namespace Palette
{
	static const FLinearColor Asphalt(0.04f, 0.04f, 0.045f);
	static const FLinearColor TrackPaint(0.09f, 0.085f, 0.08f);
	static const FLinearColor Concrete(0.35f, 0.33f, 0.3f);
	static const FLinearColor Red(0.7f, 0.05f, 0.03f);
	static const FLinearColor White(0.85f, 0.85f, 0.85f);
	static const FLinearColor Ramp(0.95f, 0.65f, 0.f);
	static const FLinearColor Steel(0.15f, 0.3f, 0.45f);
	static const FLinearColor Salt(0.72f, 0.69f, 0.62f);
	static const FLinearColor SaltTrack(0.5f, 0.46f, 0.4f);
	static const FLinearColor Rock(0.35f, 0.22f, 0.12f);
	static const FLinearColor DockGround(0.22f, 0.22f, 0.23f);
	static const FLinearColor Sidewalk(0.4f, 0.4f, 0.4f);
}

AVMArenaBuilder::AVMArenaBuilder()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	CubeMesh = VMUtil::Shape(TEXT("Cube"));
	CylinderMesh = VMUtil::Shape(TEXT("Cylinder"));
	ConeMesh = VMUtil::Shape(TEXT("Cone"));
}

// ---------------------------------------------------------------------------------------------
// Primitives
// ---------------------------------------------------------------------------------------------

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

UStaticMeshComponent* AVMArenaBuilder::AddBox(const FVector& Center, const FVector& SizeCm, const FLinearColor& Color, float Yaw, bool bCollide)
{
	return AddShape(CubeMesh, FTransform(FRotator(0.f, Yaw, 0.f), Center, SizeCm / 100.f), Color, bCollide);
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

void AVMArenaBuilder::AddTrackRamp(float Distance, float LateralFraction, float Length, float Width, float PitchDeg)
{
	FVector Tangent;
	const FVector P = TrackPointAt(Distance, &Tangent);
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Tangent);
	AddRamp(P + Right * TrackHalfWidth * LateralFraction - Tangent * Length * 0.5f, Tangent.Rotation().Yaw, Length, Width, PitchDeg, Palette::Ramp);
}

void AVMArenaBuilder::SpawnPickup(const FVector& Location)
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	GetWorld()->SpawnActor<AVMPickup>(Location, FRotator::ZeroRotator, Params);
}

// ---------------------------------------------------------------------------------------------
// Track geometry
// ---------------------------------------------------------------------------------------------

TArray<FVector> AVMArenaBuilder::MakeEllipse(float RX, float RY, int32 Samples)
{
	TArray<FVector> Points;
	for (int32 i = 0; i < Samples; ++i)
	{
		const float T = 2.f * PI * i / Samples;
		Points.Emplace(RX * FMath::Cos(T), RY * FMath::Sin(T), 0.f);
	}
	return Points;
}

TArray<FVector> AVMArenaBuilder::MakeFigureEight(float RX, float RY, int32 Samples)
{
	// Lemniscate of Gerono: crosses itself once at the origin.
	TArray<FVector> Points;
	for (int32 i = 0; i < Samples; ++i)
	{
		const float T = 2.f * PI * i / Samples;
		Points.Emplace(RX * FMath::Cos(T), RY * FMath::Sin(T) * FMath::Cos(T) * 2.f, 0.f);
	}
	return Points;
}

TArray<FVector> AVMArenaBuilder::MakeRoundedRect(float HX, float HY, float Radius, int32 SamplesPerCorner)
{
	// Starts mid-way up the right-hand straight, runs counter-clockwise.
	TArray<FVector> Points;
	Points.Emplace(HX, 0.f, 0.f);
	const FVector2D Centers[4] = {
		FVector2D(HX - Radius, HY - Radius), FVector2D(-HX + Radius, HY - Radius),
		FVector2D(-HX + Radius, -HY + Radius), FVector2D(HX - Radius, -HY + Radius)
	};
	for (int32 Corner = 0; Corner < 4; ++Corner)
	{
		for (int32 i = 0; i <= SamplesPerCorner; ++i)
		{
			const float A = (Corner * 90.f + 90.f * i / SamplesPerCorner) * PI / 180.f;
			Points.Emplace(Centers[Corner].X + Radius * FMath::Cos(A), Centers[Corner].Y + Radius * FMath::Sin(A), 0.f);
		}
	}
	return Points;
}

void AVMArenaBuilder::SetTrack(const TArray<FVector>& Points, float HalfWidth)
{
	Track = Points;
	TrackHalfWidth = HalfWidth;
	TrackCumulative.Reset();
	TrackLength = 0.f;
	for (int32 i = 0; i < Track.Num(); ++i)
	{
		TrackCumulative.Add(TrackLength);
		TrackLength += FVector::Dist(Track[i], Track[(i + 1) % Track.Num()]);
	}
}

FVector AVMArenaBuilder::TrackPointAt(float Distance, FVector* OutTangent) const
{
	if (Track.Num() < 2)
	{
		return FVector::ZeroVector;
	}
	Distance = FMath::Fmod(Distance, TrackLength);
	if (Distance < 0.f)
	{
		Distance += TrackLength;
	}
	int32 Seg = 0;
	while (Seg + 1 < Track.Num() && TrackCumulative[Seg + 1] <= Distance)
	{
		++Seg;
	}
	const FVector& A = Track[Seg];
	const FVector& B = Track[(Seg + 1) % Track.Num()];
	const float SegLen = FMath::Max(1.f, FVector::Dist(A, B));
	if (OutTangent)
	{
		*OutTangent = (B - A).GetSafeNormal();
	}
	return FMath::Lerp(A, B, (Distance - TrackCumulative[Seg]) / SegLen);
}

float AVMArenaBuilder::DistanceToTrack(const FVector& Point) const
{
	float Best = TNumericLimits<float>::Max();
	const FVector P(Point.X, Point.Y, 0.f);
	for (int32 i = 0; i < Track.Num(); ++i)
	{
		const FVector Closest = FMath::ClosestPointOnSegment(P, Track[i], Track[(i + 1) % Track.Num()]);
		Best = FMath::Min(Best, FVector::Dist(P, Closest));
	}
	return Best;
}

bool AVMArenaBuilder::IsClearOfTrack(const FVector& Point, float Radius) const
{
	return DistanceToTrack(Point) > TrackHalfWidth + Radius;
}

FVector AVMArenaBuilder::RandomPointInBounds(float Margin)
{
	return FVector(Rng.FRandRange(-HalfX + Margin, HalfX - Margin), Rng.FRandRange(-HalfY + Margin, HalfY - Margin), 0.f);
}

// ---------------------------------------------------------------------------------------------
// Shared pieces
// ---------------------------------------------------------------------------------------------

void AVMArenaBuilder::BuildGroundAndWalls(const FLinearColor& Ground, const FLinearColor& Wall, float WallHeight)
{
	AddBox(FVector(0.f, 0.f, -50.f), FVector(HalfX * 2.f, HalfY * 2.f, 100.f), Ground);
	const float T = 200.f;
	AddBox(FVector(0.f, HalfY + T * 0.5f, WallHeight * 0.5f), FVector(HalfX * 2.f + 2.f * T, T, WallHeight), Wall);
	AddBox(FVector(0.f, -HalfY - T * 0.5f, WallHeight * 0.5f), FVector(HalfX * 2.f + 2.f * T, T, WallHeight), Wall);
	AddBox(FVector(HalfX + T * 0.5f, 0.f, WallHeight * 0.5f), FVector(T, HalfY * 2.f, WallHeight), Wall);
	AddBox(FVector(-HalfX - T * 0.5f, 0.f, WallHeight * 0.5f), FVector(T, HalfY * 2.f, WallHeight), Wall);
}

void AVMArenaBuilder::PaintTrack(const FLinearColor& Color)
{
	for (int32 i = 0; i < Track.Num(); ++i)
	{
		const FVector A = Track[i];
		const FVector B = Track[(i + 1) % Track.Num()];
		const float Len = FVector::Dist(A, B) + 80.f;
		AddBox((A + B) * 0.5f + FVector(0.f, 0.f, 1.f), FVector(Len, TrackHalfWidth * 2.f, 2.f), Color, (B - A).Rotation().Yaw, false);
	}
}

void AVMArenaBuilder::SpawnCheckpoints()
{
	for (int32 i = 0; i < NumCheckpoints; ++i)
	{
		FVector Tangent;
		const FVector P = TrackPointAt(TrackLength * i / NumCheckpoints, &Tangent);
		const FTransform T(FRotator(0.f, Tangent.Rotation().Yaw, 0.f), P);
		AVMCheckpoint* CP = GetWorld()->SpawnActorDeferred<AVMCheckpoint>(AVMCheckpoint::StaticClass(), T, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		CP->Index = i;
		CP->GateHalfWidth = TrackHalfWidth;
		CP->FinishSpawning(T);
		Checkpoints.Add(CP);
	}
}

void AVMArenaBuilder::SpawnPickups(int32 OnTrack, int32 OffTrack)
{
	// Keep the start grid clear.
	const float GridClear = 2500.f;
	for (int32 i = 0; i < OnTrack; ++i)
	{
		FVector Tangent;
		const FVector P = TrackPointAt(Rng.FRandRange(GridClear, TrackLength - 300.f), &Tangent);
		const FVector Right = FVector::CrossProduct(FVector::UpVector, Tangent);
		SpawnPickup(P + Right * TrackHalfWidth * Rng.FRandRange(-0.7f, 0.7f));
	}
	for (int32 i = 0; i < OffTrack; ++i)
	{
		FVector P;
		if (VMUtil::GroundAt(GetWorld(), RandomPointInBounds(1500.f), P) && P.Z < 60.f)
		{
			SpawnPickup(P);
		}
	}
}

// ---------------------------------------------------------------------------------------------
// Arenas
// ---------------------------------------------------------------------------------------------

void AVMArenaBuilder::Build()
{
	if (bBuilt)
	{
		return;
	}
	bBuilt = true;
	Rng.Initialize(RandomSeed);

	switch (ArenaType)
	{
	case EVMArena::SaltFlats: BuildSaltFlats(); break;
	case EVMArena::Dockyard:  BuildDockyard(); break;
	case EVMArena::Downtown:  BuildDowntown(); break;
	default:                  BuildLot(); break;
	}
}

void AVMArenaBuilder::BuildLot()
{
	const float RX = 14000.f;
	const float RY = 9000.f;
	SetTrack(MakeEllipse(RX, RY, 72), 2500.f);
	HalfX = RX + TrackHalfWidth + 2500.f;
	HalfY = RY + TrackHalfWidth + 2500.f;
	PedestrianCount = 60;

	BuildGroundAndWalls(Palette::Asphalt, Palette::Concrete, 500.f);
	PaintTrack(Palette::TrackPaint);

	// Inner barrier with gaps for shortcuts into the plaza.
	const int32 Segments = 48;
	const float IX = RX - TrackHalfWidth - 100.f;
	const float IY = RY - TrackHalfWidth - 100.f;
	for (int32 i = 0; i < Segments; ++i)
	{
		if (i % 6 == 3)
		{
			continue;
		}
		const float T0 = 2.f * PI * i / Segments;
		const float T1 = 2.f * PI * (i + 1) / Segments;
		const FVector P0(IX * FMath::Cos(T0), IY * FMath::Sin(T0), 0.f);
		const FVector P1(IX * FMath::Cos(T1), IY * FMath::Sin(T1), 0.f);
		AddBox((P0 + P1) * 0.5f + FVector(0.f, 0.f, 60.f), FVector(FVector::Dist(P0, P1) + 10.f, 80.f, 120.f), (i % 2) ? Palette::Red : Palette::White, (P1 - P0).Rotation().Yaw);
	}

	// Plaza: facing ramps make a gap jump, plus a side kicker and pillars.
	AddRamp(FVector(-2800.f, 0.f, 0.f), 0.f, 1600.f, 900.f, 14.f, Palette::Ramp);
	AddRamp(FVector(2800.f, 0.f, 0.f), 180.f, 1600.f, 900.f, 14.f, Palette::Ramp);
	AddRamp(FVector(0.f, -3200.f, 0.f), 90.f, 1200.f, 700.f, 18.f, Palette::Ramp);
	for (int32 i = 0; i < 10; ++i)
	{
		const float Theta = Rng.FRandRange(0.f, 2.f * PI);
		const float Scale = Rng.FRandRange(0.25f, 0.6f);
		FVector P(IX * Scale * FMath::Cos(Theta), IY * Scale * FMath::Sin(Theta), 200.f);
		if (FMath::Abs(P.Y) < 900.f)
		{
			P.Y = FMath::Sign(P.Y + 0.1f) * 1400.f;
		}
		AddShape(CylinderMesh, FTransform(FRotator::ZeroRotator, P, FVector(1.8f, 1.8f, 4.f)), Palette::Steel);
	}

	// Kickers on the long straights, off the racing line.
	AddTrackRamp(TrackLength * 0.25f, 0.55f, 1000.f, 600.f, 12.f);
	AddTrackRamp(TrackLength * 0.75f, 0.55f, 1000.f, 600.f, 12.f);

	SpawnCheckpoints();
	SpawnPickups(14, 4);
}

void AVMArenaBuilder::BuildSaltFlats()
{
	const float RX = 18000.f;
	const float RY = 10000.f;
	SetTrack(MakeFigureEight(RX, RY, 96), 2600.f);
	HalfX = RX + TrackHalfWidth + 5000.f;
	HalfY = RY + TrackHalfWidth + 5000.f;
	PedestrianCount = 45;

	BuildGroundAndWalls(Palette::Salt, Palette::Rock, 300.f);
	PaintTrack(Palette::SaltTrack);

	// Wind-carved rock spires scattered across the flats, kept off the racing line.
	int32 Placed = 0;
	for (int32 Attempt = 0; Attempt < 400 && Placed < 45; ++Attempt)
	{
		const FVector P = RandomPointInBounds(1200.f);
		if (!IsClearOfTrack(P, 600.f))
		{
			continue;
		}
		const float Size = Rng.FRandRange(2.f, 6.f);
		const float Height = Rng.FRandRange(3.f, 14.f);
		UStaticMesh* Mesh = Rng.FRand() < 0.5f ? ConeMesh.Get() : CylinderMesh.Get();
		AddShape(Mesh, FTransform(FRotator(0.f, Rng.FRandRange(0.f, 360.f), 0.f), P + FVector(0.f, 0.f, Height * 50.f), FVector(Size, Size * Rng.FRandRange(0.7f, 1.3f), Height)), Palette::Rock * Rng.FRandRange(0.7f, 1.2f));
		++Placed;
	}

	// Big optional jumps on each lobe.
	AddTrackRamp(TrackLength * 0.06f, 0.45f, 1600.f, 900.f, 15.f);
	AddTrackRamp(TrackLength * 0.44f, -0.45f, 1600.f, 900.f, 15.f);
	AddTrackRamp(TrackLength * 0.56f, 0.45f, 1600.f, 900.f, 15.f);
	AddTrackRamp(TrackLength * 0.94f, -0.45f, 1600.f, 900.f, 15.f);

	// Marker cones at the crossroads.
	for (int32 i = 0; i < 8; ++i)
	{
		const float A = 2.f * PI * i / 8.f;
		AddShape(ConeMesh, FTransform(FRotator::ZeroRotator, FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * (TrackHalfWidth + 900.f) + FVector(0.f, 0.f, 75.f), FVector(0.8f, 0.8f, 1.5f)), Palette::Red, false);
	}

	SpawnCheckpoints();
	SpawnPickups(16, 8);
}

void AVMArenaBuilder::BuildDockyard()
{
	const float HX = 15000.f;
	const float HY = 10000.f;
	SetTrack(MakeRoundedRect(HX, HY, 3500.f, 8), 2200.f);
	HalfX = HX + TrackHalfWidth + 3000.f;
	HalfY = HY + TrackHalfWidth + 3000.f;
	PedestrianCount = 55;

	BuildGroundAndWalls(Palette::DockGround, Palette::Concrete, 450.f);
	PaintTrack(Palette::TrackPaint);

	const FLinearColor ContainerColors[] = {
		FLinearColor(0.55f, 0.08f, 0.05f), FLinearColor(0.05f, 0.2f, 0.5f), FLinearColor(0.1f, 0.4f, 0.15f),
		FLinearColor(0.8f, 0.4f, 0.05f), FLinearColor(0.5f, 0.5f, 0.52f), FLinearColor(0.6f, 0.55f, 0.1f)
	};
	auto Container = [&](const FVector& Base, float Yaw, int32 Stack)
	{
		const FVector Size(1200.f, 500.f, 500.f);
		for (int32 Level = 0; Level < Stack; ++Level)
		{
			const FLinearColor C = ContainerColors[Rng.RandRange(0, static_cast<int32>(UE_ARRAY_COUNT(ContainerColors)) - 1)];
			AddBox(Base + FVector(0.f, 0.f, Size.Z * (Level + 0.5f)), Size, C, Yaw + Rng.FRandRange(-3.f, 3.f));
		}
	};

	// Container yard inside the loop: rows with aisles, some gaps so you can carve through.
	const float InnerX = HX - TrackHalfWidth - 900.f;
	const float InnerY = HY - TrackHalfWidth - 900.f;
	for (float Y = -InnerY + 400.f; Y <= InnerY - 400.f; Y += 1700.f)
	{
		for (float X = -InnerX + 700.f; X <= InnerX - 700.f; X += 1350.f)
		{
			// Two clear lanes through the middle make cross-yard shortcuts.
			if (FMath::Abs(Y) < 900.f || FMath::Abs(X) < 1000.f || Rng.FRand() < 0.25f)
			{
				continue;
			}
			Container(FVector(X, Y, 0.f), 0.f, Rng.RandRange(1, 3));
		}
	}

	// Stacks around the outside of the loop.
	for (int32 i = 0; i < 40; ++i)
	{
		const FVector P = RandomPointInBounds(700.f);
		if (IsClearOfTrack(P, 900.f) && (FMath::Abs(P.X) > HX + TrackHalfWidth || FMath::Abs(P.Y) > HY + TrackHalfWidth))
		{
			Container(P, Rng.RandBool() ? 0.f : 90.f, Rng.RandRange(1, 4));
		}
	}

	// Gantry cranes straddling the long straights.
	for (float Side : { -1.f, 1.f })
	{
		const float Y = Side * HY;
		const float CraneX = Side * 4000.f;
		const float Legs = TrackHalfWidth + 500.f;
		for (float Leg : { -1.f, 1.f })
		{
			AddShape(CylinderMesh, FTransform(FRotator::ZeroRotator, FVector(CraneX, Y + Leg * Legs, 1800.f), FVector(1.2f, 1.2f, 36.f)), FLinearColor(0.9f, 0.6f, 0.05f));
		}
		AddBox(FVector(CraneX, Y, 3700.f), FVector(400.f, Legs * 2.f + 600.f, 300.f), FLinearColor(0.9f, 0.6f, 0.05f), 0.f, false);
		AddBox(FVector(CraneX + 1500.f, Y, 3950.f), FVector(4000.f, 250.f, 200.f), FLinearColor(0.9f, 0.6f, 0.05f), 0.f, false);
	}

	AddTrackRamp(TrackLength * 0.3f, -0.4f, 1200.f, 700.f, 13.f);
	AddTrackRamp(TrackLength * 0.8f, -0.4f, 1200.f, 700.f, 13.f);

	SpawnCheckpoints();
	SpawnPickups(14, 6);
}

void AVMArenaBuilder::BuildDowntown()
{
	const int32 NX = 4;
	const int32 NY = 3;
	const float Block = 4500.f;
	const float Street = 2600.f;
	const float TotalX = NX * Block + (NX + 1) * Street;
	const float TotalY = NY * Block + (NY + 1) * Street;

	// The race runs round the ring road; the side streets are shortcuts and hunting grounds.
	SetTrack(MakeRoundedRect(TotalX * 0.5f - Street * 0.5f, TotalY * 0.5f - Street * 0.5f, Street * 0.5f, 6), Street * 0.5f);
	HalfX = TotalX * 0.5f + 300.f;
	HalfY = TotalY * 0.5f + 300.f;
	PedestrianCount = 90;

	BuildGroundAndWalls(Palette::Asphalt, FLinearColor(0.25f, 0.22f, 0.2f), 600.f);

	const FLinearColor Facades[] = {
		FLinearColor(0.45f, 0.35f, 0.28f), FLinearColor(0.3f, 0.32f, 0.36f), FLinearColor(0.5f, 0.48f, 0.42f),
		FLinearColor(0.25f, 0.18f, 0.15f), FLinearColor(0.2f, 0.25f, 0.3f), FLinearColor(0.55f, 0.5f, 0.45f)
	};

	for (int32 BX = 0; BX < NX; ++BX)
	{
		for (int32 BY = 0; BY < NY; ++BY)
		{
			const FVector Center(-TotalX * 0.5f + Street + BX * (Block + Street) + Block * 0.5f,
				-TotalY * 0.5f + Street + BY * (Block + Street) + Block * 0.5f, 0.f);

			// Sidewalk slab: a low kerb pedestrians walk on.
			AddBox(Center + FVector(0.f, 0.f, 6.f), FVector(Block, Block, 12.f), Palette::Sidewalk);

			// Up to four buildings per block; a missing one becomes an open plaza.
			const float Lot = (Block - 900.f) * 0.5f;
			for (int32 LX = 0; LX < 2; ++LX)
			{
				for (int32 LY = 0; LY < 2; ++LY)
				{
					if (Rng.FRand() < 0.2f)
					{
						continue;
					}
					const FVector LotCenter = Center + FVector((LX - 0.5f) * (Lot + 150.f), (LY - 0.5f) * (Lot + 150.f), 0.f);
					const float Height = Rng.FRandRange(800.f, 4500.f);
					const FLinearColor C = Facades[Rng.RandRange(0, static_cast<int32>(UE_ARRAY_COUNT(Facades)) - 1)];
					AddBox(LotCenter + FVector(0.f, 0.f, Height * 0.5f + 12.f), FVector(Lot, Lot, Height), C);
					// Roof trim.
					AddBox(LotCenter + FVector(0.f, 0.f, Height + 40.f), FVector(Lot + 60.f, Lot + 60.f, 60.f), C * 0.6f, 0.f, false);
				}
			}

			// Street lights on each corner (decorative, cars pass through).
			for (float SX : { -1.f, 1.f })
			{
				for (float SY : { -1.f, 1.f })
				{
					const FVector Corner = Center + FVector(SX * (Block * 0.5f - 150.f), SY * (Block * 0.5f - 150.f), 0.f);
					AddShape(CylinderMesh, FTransform(FRotator::ZeroRotator, Corner + FVector(0.f, 0.f, 300.f), FVector(0.15f, 0.15f, 6.f)), FLinearColor(0.1f, 0.1f, 0.1f), false);
					AddShape(VMUtil::Shape(TEXT("Sphere")), FTransform(FRotator::ZeroRotator, Corner + FVector(0.f, 0.f, 620.f), FVector(0.4f)), FLinearColor(1.f, 0.9f, 0.6f), false);
				}
			}
		}
	}

	// Road paint: centre dashes on the ring road.
	for (int32 i = 0; i < 120; ++i)
	{
		FVector Tangent;
		const FVector P = TrackPointAt(TrackLength * i / 120.f, &Tangent);
		AddBox(P + FVector(0.f, 0.f, 1.f), FVector(400.f, 40.f, 2.f), FLinearColor(0.8f, 0.7f, 0.1f), Tangent.Rotation().Yaw, false);
	}

	SpawnCheckpoints();
	SpawnPickups(12, 10);
}

// ---------------------------------------------------------------------------------------------
// Queries and lighting
// ---------------------------------------------------------------------------------------------

FVector AVMArenaBuilder::GetRandomGroundPoint(FRandomStream& InRng) const
{
	FVector Ground = FVector::ZeroVector;
	for (int32 Attempt = 0; Attempt < 12; ++Attempt)
	{
		const FVector P(InRng.FRandRange(-HalfX + 600.f, HalfX - 600.f), InRng.FRandRange(-HalfY + 600.f, HalfY - 600.f), 0.f);
		if (VMUtil::GroundAt(GetWorld(), P, Ground) && Ground.Z < 60.f)
		{
			return Ground;
		}
	}
	// Fall back to somewhere on the track, which is always open ground.
	return TrackPointAt(InRng.FRandRange(0.f, TrackLength));
}

void AVMArenaBuilder::EnsureLighting(UWorld* World, EVMArena Arena)
{
	for (TActorIterator<ADirectionalLight> It(World); It; ++It)
	{
		return; // The level already has its own lighting.
	}

	FRotator SunRotation(-38.f, 35.f, 0.f);
	FLinearColor SunColor = FLinearColor::White;
	float SunIntensity = 8.f;
	float FogDensity = 0.02f;
	switch (Arena)
	{
	case EVMArena::SaltFlats: SunRotation = FRotator(-60.f, 10.f, 0.f); SunIntensity = 10.f; FogDensity = 0.01f; break;
	case EVMArena::Dockyard:  SunRotation = FRotator(-30.f, 120.f, 0.f); SunIntensity = 5.f; FogDensity = 0.06f; SunColor = FLinearColor(0.85f, 0.9f, 1.f); break;
	case EVMArena::Downtown:  SunRotation = FRotator(-12.f, 250.f, 0.f); SunIntensity = 6.f; FogDensity = 0.03f; SunColor = FLinearColor(1.f, 0.7f, 0.45f); break;
	default: break;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	if (ADirectionalLight* Sun = World->SpawnActor<ADirectionalLight>(FVector(0.f, 0.f, 5000.f), SunRotation, Params))
	{
		Sun->SetMobility(EComponentMobility::Movable);
		Sun->GetLightComponent()->SetIntensity(SunIntensity);
		Sun->GetLightComponent()->SetLightColor(SunColor);
	}
	World->SpawnActor<ASkyAtmosphere>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
	if (ASkyLight* Sky = World->SpawnActor<ASkyLight>(FVector(0.f, 0.f, 3000.f), FRotator::ZeroRotator, Params))
	{
		USkyLightComponent* SkyComp = Sky->GetLightComponent();
		SkyComp->SetMobility(EComponentMobility::Movable);
		SkyComp->bRealTimeCapture = true;
		SkyComp->RecaptureSky();
	}
	if (AExponentialHeightFog* Fog = World->SpawnActor<AExponentialHeightFog>(FVector::ZeroVector, FRotator::ZeroRotator, Params))
	{
		Fog->GetComponent()->SetFogDensity(FogDensity);
	}
}
