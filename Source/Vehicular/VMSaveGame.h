#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "VMTypes.h"
#include "VMSaveGame.generated.h"

/** Career progress: credits, garage upgrades and which event you're on. */
UCLASS()
class VEHICULAR_API UVMSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY() int32 Credits = 0;
	UPROPERTY() int32 EventNumber = 1;
	UPROPERTY() int32 EventsWon = 0;
	UPROPERTY() int32 BestPoints = 0;
	UPROPERTY() FVMUpgradeLevels Upgrades;
	UPROPERTY() TArray<FName> OwnedCars;
	UPROPERTY() FName SelectedCar;

	// Settings (0..1 volumes, particle density 0.25..1.5).
	UPROPERTY() float MasterVolume = 1.f;
	UPROPERTY() float EffectsVolume = 1.f;
	UPROPERTY() float EngineVolume = 1.f;
	UPROPERTY() float ParticleDensity = 1.f;
};
