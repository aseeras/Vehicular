#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "VMTypes.h"
#include "VMAudioSettings.generated.h"

class USoundBase;

/**
 * Project Settings > Game > Vehicular Audio. Any sound left empty uses the built-in
 * synthesised version; assign a Sound Wave or MetaSound to replace it.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Vehicular Audio"))
class VEHICULAR_API UVMAudioSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(Config, EditAnywhere, Category = "Overrides")
	TMap<EVMSound, TSoftObjectPtr<USoundBase>> SoundOverrides;

	/** A looping engine sound. Its pitch is driven by RPM, so it should be recorded near idle. */
	UPROPERTY(Config, EditAnywhere, Category = "Overrides")
	TSoftObjectPtr<USoundBase> EngineLoopOverride;

	UPROPERTY(Config, EditAnywhere, Category = "Mix", meta = (ClampMin = "0", ClampMax = "2"))
	float EffectsVolume = 1.f;

	UPROPERTY(Config, EditAnywhere, Category = "Mix", meta = (ClampMin = "0", ClampMax = "2"))
	float EngineVolume = 0.7f;

	virtual FName GetCategoryName() const override { return TEXT("Game"); }
};
