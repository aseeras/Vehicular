#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "VMTypes.h"
#include "VMProfileSubsystem.generated.h"

class UVMSaveGame;

/** Owns the career save and lives for the whole session, so it survives level reloads. */
UCLASS()
class VEHICULAR_API UVMProfileSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	static UVMProfileSubsystem* Get(const UObject* WorldContext);

	int32 GetCredits() const;
	int32 GetEventNumber() const;
	int32 GetEventsWon() const;
	int32 GetBestPoints() const;
	FVMUpgradeLevels GetUpgrades() const;
	int32 GetUpgradeLevel(EVMUpgrade Upgrade) const;
	/** Cost of the next level, or -1 if maxed. */
	int32 GetNextUpgradeCost(EVMUpgrade Upgrade) const;

	bool TryPurchase(EVMUpgrade Upgrade);

	// Cars
	FName GetSelectedCar() const;
	bool IsCarOwned(FName CarId) const;
	bool SelectCar(FName CarId);
	/** Buys (and selects) a car if it's unlocked by wins and affordable. */
	bool TryBuyCar(FName CarId);

	void AwardCredits(int32 Amount);
	void RecordEvent(bool bWon, int32 Points);
	void ResetCareer();

	/** Set when leaving the garage so the reloaded level goes straight to the race. */
	void SetSkipGarageOnNextLoad(bool bSkip) { bSkipGarage = bSkip; }
	bool ConsumeSkipGarage();

private:
	void Save();
	void EnsureStarterCar();

	UPROPERTY() TObjectPtr<UVMSaveGame> Profile;
	bool bSkipGarage = false;
};
