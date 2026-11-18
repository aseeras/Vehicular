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
};
