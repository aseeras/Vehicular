#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "VMEffectsSettings.generated.h"

class UNiagaraSystem;

/** One-shot effects that can be replaced with a Niagara system. */
UENUM(BlueprintType)
enum class EVMEffect : uint8
{
	Sparks,
	Explosion,
	Splat,
	EMP
};

/**
 * Project Settings > Game > Vehicular Effects. The game draws its own particles from basic
 * shapes; assign Niagara systems here to replace the one-shot bursts with real VFX.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Vehicular Effects"))
class VEHICULAR_API UVMEffectsSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(Config, EditAnywhere, Category = "Overrides")
	TMap<EVMEffect, TSoftObjectPtr<UNiagaraSystem>> NiagaraOverrides;

	/** Scales how many procedural particles are spawned. Lower this on slow machines. */
	UPROPERTY(Config, EditAnywhere, Category = "Procedural", meta = (ClampMin = "0.1", ClampMax = "2"))
	float ParticleDensity = 1.f;

	UPROPERTY(Config, EditAnywhere, Category = "Procedural", meta = (ClampMin = "50", ClampMax = "5000"))
	int32 MaxParticlesPerLayer = 800;

	virtual FName GetCategoryName() const override { return TEXT("Game"); }
};
