#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VMArenaBuilder.generated.h"

class AVMCheckpoint;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * Builds the default arena out of basic shapes at runtime: a walled lot with an oval
 * circuit, a broken inner barrier you can cut through, a jump plaza in the middle,
 * checkpoints and pickups. Used when the loaded map has no checkpoints of its own.
 */
UCLASS()
class VEHICULAR_API AVMArenaBuilder : public AActor
{
	GENERATED_BODY()

public:
	AVMArenaBuilder();

	UPROPERTY(EditAnywhere, Category = "Arena") float TrackRadiusX = 14000.f;
	UPROPERTY(EditAnywhere, Category = "Arena") float TrackRadiusY = 9000.f;
	UPROPERTY(EditAnywhere, Category = "Arena") float TrackHalfWidth = 2500.f;
	UPROPERTY(EditAnywhere, Category = "Arena") int32 NumCheckpoints = 8;
	UPROPERTY(EditAnywhere, Category = "Arena") int32 NumTrackPickups = 14;
	UPROPERTY(EditAnywhere, Category = "Arena") int32 NumPlazaPickups = 4;
	UPROPERTY(EditAnywhere, Category = "Arena") int32 RandomSeed = 1337;

	void Build();
	const TArray<TObjectPtr<AVMCheckpoint>>& GetCheckpoints() const { return Checkpoints; }
	FVector GetRandomGroundPoint(FRandomStream& Rng) const;

	/** Adds sun, sky and fog if the level has no directional light. */
	static void EnsureLighting(UWorld* World);

private:
	UStaticMeshComponent* AddShape(UStaticMesh* Mesh, const FTransform& Transform, const FLinearColor& Color, bool bCollide = true);
	void AddRamp(const FVector& Base, float Yaw, float Length, float Width, float PitchDeg, const FLinearColor& Color);
	FVector EllipsePoint(float Theta, float Scale = 1.f) const;
	FVector EllipseTangent(float Theta) const;
	void SpawnPickup(const FVector& Location);

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TArray<TObjectPtr<AVMCheckpoint>> Checkpoints;
	UPROPERTY() TObjectPtr<UStaticMesh> CubeMesh;
	UPROPERTY() TObjectPtr<UStaticMesh> CylinderMesh;

	FRandomStream Rng;
	float HalfX = 0.f;
	float HalfY = 0.f;
	bool bBuilt = false;
};
