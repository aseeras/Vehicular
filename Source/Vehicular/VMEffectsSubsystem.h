#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "VMEffectsSettings.h"
#include "VMEffectsSubsystem.generated.h"

class UInstancedStaticMeshComponent;

/** Each layer is one colour/shape, drawn with a single instanced mesh component. */
enum class EVMParticleLayer : uint8
{
	Spark,
	Fire,
	SmokeLight,
	SmokeDark,
	Blood,
	Energy,
	Flame,
	Count
};

struct FVMParticle
{
	FVector Position = FVector::ZeroVector;
	FVector Velocity = FVector::ZeroVector;
	FRotator Rotation = FRotator::ZeroRotator;
	float Age = 0.f;
	float Life = 1.f;
	float StartSize = 10.f;  // cm
	float EndSize = 10.f;    // cm
	float Drag = 0.f;        // 1/s
	float Gravity = 0.f;     // cm/s^2, positive pulls down
	bool bStretch = false;   // stretch along velocity (sparks)
};

/**
 * Lightweight CPU particle system for sparks, smoke, fire, flames and splats, rendered with
 * instanced basic shapes, so the project needs no VFX assets.
 */
UCLASS()
class VEHICULAR_API UVMEffectsSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	static UVMEffectsSubsystem* Get(const UObject* WorldContext);

	// One-shot bursts (can be replaced by Niagara in Project Settings).
	void Sparks(const FVector& Location, const FVector& Normal, int32 Count, float Speed = 900.f);
	void Explosion(const FVector& Location);
	void Splat(const FVector& Location, const FVector& ImpactVelocity);
	void EMPBurst(const FVector& Location, float Radius);

	// Continuous emitters call these every frame they're active.
	void SmokePuff(const FVector& Location, const FVector& Velocity, bool bDark, float Size);
	void TyreSmoke(const FVector& Location, const FVector& CarVelocity);
	void FirePuff(const FVector& Location);
	void NitroFlame(const FVector& Location, const FVector& Direction, const FVector& CarVelocity);
	void OilSplash(const FVector& Location);

private:
	bool SpawnOverride(EVMEffect Effect, const FVector& Location, const FRotator& Rotation);
	int32 Scaled(int32 Count) const;
	void Emit(EVMParticleLayer Layer, const FVMParticle& Particle);
	bool EnsureHost();

	UPROPERTY() TObjectPtr<AActor> Host;
	UPROPERTY() TArray<TObjectPtr<UInstancedStaticMeshComponent>> LayerComponents;

	TArray<TArray<FVMParticle>> Layers;
	TArray<FTransform> Scratch;
	FRandomStream Rng;
};
