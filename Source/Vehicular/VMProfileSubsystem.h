#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "VMTypes.h"
#include "VMProfileSubsystem.generated.h"

class UVMSaveGame;

/** What the next level load should open into. */
enum class EVMLoadIntent : uint8
{
	MainMenu,
	CareerRace,
	QuickRace
};

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

	// Level-load routing (kept in memory across level loads, not saved).
	void SetNextLoadIntent(EVMLoadIntent Intent) { LoadIntent = Intent; }
	EVMLoadIntent PeekLoadIntent() const { return LoadIntent; }
	EVMLoadIntent ConsumeLoadIntent();

	// Quick race options (in memory).
	EVMArena QuickArena = EVMArena::Lot;
	int32 QuickDifficulty = 1;

	// Settings (saved).
	float GetMasterVolume() const;
	float GetEffectsVolume() const;
	float GetEngineVolume() const;
	float GetParticleDensity() const;
	void SetMasterVolume(float Value);
	void SetEffectsVolume(float Value);
	void SetEngineVolume(float Value);
	void SetParticleDensity(float Value);

private:
	void Save();
	void EnsureStarterCar();

	UPROPERTY() TObjectPtr<UVMSaveGame> Profile;
	EVMLoadIntent LoadIntent = EVMLoadIntent::MainMenu;
};
