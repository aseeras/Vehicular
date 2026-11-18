#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "VMTypes.h"
#include "VMGameMode.generated.h"

class AVMVehiclePawn;
class AVMCheckpoint;
class AVMPedestrian;
class AVMArenaBuilder;

USTRUCT()
struct FVMRaceProgress
{
	GENERATED_BODY()

	UPROPERTY() int32 NextCheckpoint = 1;
	UPROPERTY() int32 LapsCompleted = 0;
};

USTRUCT(BlueprintType)
struct FVMFeedMessage
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FString Text;
	UPROPERTY(BlueprintReadOnly) FLinearColor Color = FLinearColor::White;
	UPROPERTY(BlueprintReadOnly) float TimeLeft = 0.f;
};

/**
 * Match rules. Three ways to win: finish the laps first, wreck every rival, or hit the
 * points target. You lose if the clock runs out, your car is wrecked, or a rival finishes first.
 */
UCLASS()
class VEHICULAR_API AVMGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AVMGameMode();

	virtual void StartPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;

	// --- Rules ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rules") int32 NumLaps = 3;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rules") int32 TargetPoints = 15000;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rules") int32 NumRivals = 5;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rules") int32 MaxPedestrians = 60;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rules") float StartingTime = 60.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rules") float CountdownSeconds = 3.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rules|Bonuses") float CheckpointTimeBonus = 8.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rules|Bonuses") int32 CheckpointPoints = 250;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rules|Bonuses") float PedestrianTimeBonus = 2.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rules|Bonuses") int32 PedestrianPoints = 100;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rules|Bonuses") float ComboWindow = 3.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rules|Bonuses") int32 PointsPerDamage = 10;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rules|Bonuses") float TimePerDamage = 0.1f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rules|Bonuses") int32 WreckPoints = 2500;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rules|Bonuses") float WreckTimeBonus = 15.f;

	/** Credits = points / this, plus a win bonus. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rules|Career") int32 PointsPerCredit = 5;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rules|Career") int32 WinCreditBonus = 1000;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rules|Career") int32 WinCreditBonusPerEvent = 250;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rules|Career") int32 MaxRivals = 8;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rules|Career") int32 TargetPointsPerEvent = 2500;

	/** Class used for rival cars. Set to a Blueprint subclass to give rivals your own mesh. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Classes") TSubclassOf<AVMVehiclePawn> RivalClass;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Classes") TSubclassOf<AVMPedestrian> PedestrianClass;

	// --- Events from gameplay actors ---
	void OnCheckpointReached(AVMVehiclePawn* Vehicle, int32 Index);
	void OnPedestrianKilled(AVMPedestrian* Pedestrian, AVMVehiclePawn* Killer);
	void OnVehicleDamaged(AVMVehiclePawn* Victim, AVMVehiclePawn* Attacker, float Amount);
	void OnVehicleWrecked(AVMVehiclePawn* Victim, AVMVehiclePawn* Attacker);

	void AddTime(float Seconds);
	void AddPoints(int32 Amount);
	void PushMessage(const FString& Text, const FLinearColor& Color = FLinearColor::White, float Duration = 3.f);
	void RestartMatch();

	// --- Garage ---
	/** Confirm button: results -> garage; in the garage buys the selected upgrade or starts the event. */
	void HandleConfirm();
	void NavigateGarage(int32 Direction);
	/** Left/right: browses cars when the car row is selected. */
	void NavigateGarageSide(int32 Direction);
	void OpenGarage();
	int32 GetGarageSelection() const { return GarageSelection; }
	/** Upgrades, then the car row, then "start event". */
	int32 GetNumGarageItems() const { return VMUpgrades::NumUpgrades + 2; }
	int32 GetCarRowIndex() const { return VMUpgrades::NumUpgrades; }
	int32 GetGarageCarIndex() const { return GarageCarIndex; }
	int32 GetCreditsEarned() const { return CreditsEarned; }
	int32 GetEventNumber() const { return EventNumber; }
	int32 GetRivalTier() const { return RivalTier; }
	const FString& GetGarageNotice() const { return GarageNotice; }
	/** Difficulty preview for an event number (used by the garage screen). */
	void GetEventPreview(int32 InEvent, int32& OutRivals, int32& OutTier, int32& OutTarget) const;

	// --- Queries ---
	EVMMatchState GetMatchState() const { return State; }
	bool IsRaceLive() const { return State == EVMMatchState::Racing; }
	bool IsMatchOver() const { return State == EVMMatchState::Won || State == EVMMatchState::Lost; }
	bool IsInGarage() const { return State == EVMMatchState::Garage; }
	float GetTimeLeft() const { return TimeLeft; }
	float GetCountdownLeft() const { return CountdownLeft; }
	int32 GetPoints() const { return Points; }
	int32 GetCombo() const { return Combo; }
	int32 GetPedestriansKilled() const { return PedestriansKilled; }
	int32 GetRivalsWrecked() const { return RivalsWrecked; }
	const FString& GetResultText() const { return ResultText; }
	const TArray<FVMFeedMessage>& GetMessages() const { return Messages; }
	int32 GetNumCheckpoints() const { return Checkpoints.Num(); }

	AVMVehiclePawn* GetPlayerVehicle() const;
	TArray<AVMVehiclePawn*> GetAllVehicles() const;
	const TArray<TObjectPtr<AVMVehiclePawn>>& GetRivals() const { return Rivals; }

	AVMCheckpoint* GetNextCheckpointFor(const AVMVehiclePawn* Vehicle) const;
	AVMCheckpoint* GetCheckpointAfter(const AVMCheckpoint* Checkpoint) const;
	int32 GetLapFor(const AVMVehiclePawn* Vehicle) const;
	int32 GetRacePosition(const AVMVehiclePawn* Vehicle) const;

private:
	void EnsureArena();
	void ConfigureEvent();
	FTransform GetGridTransform(int32 Slot) const;
	void SpawnRivals();
	void SpawnPedestrian();
	void MaintainPedestrians(float DeltaSeconds);
	FVector RandomGroundPoint();
	float ProgressScore(const AVMVehiclePawn* Vehicle) const;
	FVMRaceProgress GetProgress(const AVMVehiclePawn* Vehicle) const;
	void EndMatch(bool bWon, const FString& Reason);

	UPROPERTY() TObjectPtr<AVMArenaBuilder> Arena;
	UPROPERTY() TArray<TObjectPtr<AVMCheckpoint>> Checkpoints;
	UPROPERTY() TArray<TObjectPtr<AVMVehiclePawn>> Rivals;
	UPROPERTY() TArray<TObjectPtr<AVMPedestrian>> Pedestrians;
	UPROPERTY() TMap<TObjectPtr<AVMVehiclePawn>, FVMRaceProgress> Progress;

	TArray<FVMFeedMessage> Messages;
	FRandomStream Rng;

	EVMMatchState State = EVMMatchState::Countdown;
	FString ResultText;
	float TimeLeft = 60.f;
	float CountdownLeft = 3.f;
	float PedestrianSpawnTimer = 0.f;
	double LastKillTime = -100.0;
	int32 Points = 0;
	int32 Combo = 0;
	int32 PedestriansKilled = 0;
	int32 RivalsWrecked = 0;
	int32 HighlightedCheckpoint = -1;
	bool bArenaReady = false;
	bool bEventConfigured = false;

	int32 BaseNumRivals = 5;
	int32 BaseTargetPoints = 15000;
	int32 EventNumber = 1;
	int32 RivalTier = 0;
	int32 CreditsEarned = 0;
	int32 GarageSelection = 0;
	int32 GarageCarIndex = 0;
	int32 LastCountdownBeep = -1;
	FString GarageNotice;
};
