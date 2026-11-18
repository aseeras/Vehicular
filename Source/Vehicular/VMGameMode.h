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

	// --- Queries ---
	EVMMatchState GetMatchState() const { return State; }
	bool IsRaceLive() const { return State == EVMMatchState::Racing; }
	bool IsMatchOver() const { return State == EVMMatchState::Won || State == EVMMatchState::Lost; }
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
};
