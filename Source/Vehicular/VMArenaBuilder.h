#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VMTypes.h"
#include "VMArenaBuilder.generated.h"

class AVMCheckpoint;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * Builds one of the built-in arenas out of basic shapes at runtime. Each arena is a closed
 * track (a centre-line polyline) plus its own scenery. Used when the loaded level has no
 * checkpoints of its own.
 */
UCLASS()
class VEHICULAR_API AVMArenaBuilder : public AActor
{
	GENERATED_BODY()

public:
	AVMArenaBuilder();

	UPROPERTY(EditAnywhere, Category = "Arena") EVMArena ArenaType = EVMArena::Lot;
	UPROPERTY(EditAnywhere, Category = "Arena") int32 NumCheckpoints = 8;
	UPROPERTY(EditAnywhere, Category = "Arena") int32 RandomSeed = 1337;

	void Build();
	const TArray<TObjectPtr<AVMCheckpoint>>& GetCheckpoints() const { return Checkpoints; }
	/** A random spot at street level (never on a rooftop or container). */
	FVector GetRandomGroundPoint(FRandomStream& Rng) const;
	int32 GetPedestrianCount() const { return PedestrianCount; }

	/** Adds sun, sky and fog suited to the arena if the level has no directional light. */
	static void EnsureLighting(UWorld* World, EVMArena Arena);

private:
	// Arena recipes.
	void BuildLot();
	void BuildSaltFlats();
	void BuildDockyard();
	void BuildDowntown();

	// Shared building blocks.
	void BuildGroundAndWalls(const FLinearColor& Ground, const FLinearColor& Wall, float WallHeight);
	void PaintTrack(const FLinearColor& Color);
	void SpawnCheckpoints();
	void SpawnPickups(int32 OnTrack, int32 OffTrack);
	void SpawnPickup(const FVector& Location);
	void AddRamp(const FVector& Base, float Yaw, float Length, float Width, float PitchDeg, const FLinearColor& Color);
	void AddTrackRamp(float Distance, float LateralFraction, float Length, float Width, float PitchDeg);
	UStaticMeshComponent* AddShape(UStaticMesh* Mesh, const FTransform& Transform, const FLinearColor& Color, bool bCollide = true);
	UStaticMeshComponent* AddBox(const FVector& Center, const FVector& SizeCm, const FLinearColor& Color, float Yaw = 0.f, bool bCollide = true);

	// Track geometry.
	void SetTrack(const TArray<FVector>& Points, float HalfWidth);
	static TArray<FVector> MakeEllipse(float RX, float RY, int32 Samples);
	static TArray<FVector> MakeFigureEight(float RX, float RY, int32 Samples);
	static TArray<FVector> MakeRoundedRect(float HX, float HY, float Radius, int32 SamplesPerCorner);
	FVector TrackPointAt(float Distance, FVector* OutTangent = nullptr) const;
	float DistanceToTrack(const FVector& Point) const;
	bool IsClearOfTrack(const FVector& Point, float Radius) const;
	FVector RandomPointInBounds(float Margin);

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TArray<TObjectPtr<AVMCheckpoint>> Checkpoints;
	UPROPERTY() TObjectPtr<UStaticMesh> CubeMesh;
	UPROPERTY() TObjectPtr<UStaticMesh> CylinderMesh;
	UPROPERTY() TObjectPtr<UStaticMesh> ConeMesh;

	TArray<FVector> Track;
	TArray<float> TrackCumulative;
	float TrackLength = 0.f;
	float TrackHalfWidth = 2500.f;
	float HalfX = 20000.f;
	float HalfY = 15000.f;
	int32 PedestrianCount = 60;

	FRandomStream Rng;
	bool bBuilt = false;
};
