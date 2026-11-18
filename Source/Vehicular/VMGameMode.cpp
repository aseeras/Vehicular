#include "VMGameMode.h"
#include "Vehicular.h"
#include "VMArenaBuilder.h"
#include "VMCheckpoint.h"
#include "VMDamageComponent.h"
#include "VMHUD.h"
#include "VMPedestrian.h"
#include "VMPlayerController.h"
#include "VMRivalAIController.h"
#include "VMUtil.h"
#include "VMVehiclePawn.h"

#include "Algo/Sort.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	const TCHAR* RivalNames[] = {
		TEXT("Grinder"), TEXT("Mad Maddie"), TEXT("Sawtooth"), TEXT("Old Crusty"),
		TEXT("Nitro Nan"), TEXT("The Dentist"), TEXT("Wrecking Rex"), TEXT("Lady Lugnut")
	};
	const FLinearColor RivalColors[] = {
		FLinearColor(1.f, 0.1f, 0.05f), FLinearColor(1.f, 0.85f, 0.f), FLinearColor(0.f, 0.9f, 1.f), FLinearColor(0.6f, 0.3f, 0.1f),
		FLinearColor(0.2f, 0.4f, 1.f), FLinearColor(1.f, 1.f, 1.f), FLinearColor(1.f, 0.4f, 0.f), FLinearColor(1.f, 0.2f, 0.8f)
	};
	constexpr int32 NumRivalPresets = UE_ARRAY_COUNT(RivalNames);

	const FLinearColor Good(0.3f, 1.f, 0.35f);
	const FLinearColor Bad(1.f, 0.25f, 0.2f);
	const FLinearColor Carnage(1.f, 0.55f, 0.1f);
}

AVMGameMode::AVMGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	DefaultPawnClass = AVMVehiclePawn::StaticClass();
	PlayerControllerClass = AVMPlayerController::StaticClass();
	HUDClass = AVMHUD::StaticClass();
	RivalClass = AVMVehiclePawn::StaticClass();
	PedestrianClass = AVMPedestrian::StaticClass();
}

AActor* AVMGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	// The player can spawn before StartPlay, so make sure the arena and start grid exist.
	EnsureArena();
	return Super::ChoosePlayerStart_Implementation(Player);
}

void AVMGameMode::EnsureArena()
{
	if (bArenaReady)
	{
		return;
	}
	bArenaReady = true;

	UWorld* World = GetWorld();
	Rng.Initialize(FMath::Rand());

	for (TActorIterator<AVMCheckpoint> It(World); It; ++It)
	{
		Checkpoints.Add(*It);
	}

	if (Checkpoints.Num() == 0)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Arena = World->SpawnActor<AVMArenaBuilder>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
		Arena->Build();
		Checkpoints = Arena->GetCheckpoints();
	}

	Algo::Sort(Checkpoints, [](const TObjectPtr<AVMCheckpoint>& A, const TObjectPtr<AVMCheckpoint>& B)
	{
		return A->Index < B->Index;
	});

	AVMArenaBuilder::EnsureLighting(World);

	bool bHasPlayerStart = false;
	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		bHasPlayerStart = true;
		break;
	}
	if (!bHasPlayerStart && Checkpoints.Num() > 0)
	{
		const FTransform Start = GetGridTransform(0);
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		World->SpawnActor<APlayerStart>(Start.GetLocation(), Start.Rotator(), Params);
	}

	if (Checkpoints.Num() < 2)
	{
		UE_LOG(LogVehicular, Warning, TEXT("Fewer than 2 checkpoints in the level - lap racing is disabled."));
	}
}

FTransform AVMGameMode::GetGridTransform(int32 Slot) const
{
	if (Checkpoints.Num() == 0)
	{
		return FTransform(FVector(Slot * 600.f, 0.f, 200.f));
	}
	const AVMCheckpoint* Start = Checkpoints[0];
	const FVector Forward = Start->GetActorForwardVector().GetSafeNormal2D();
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);
	const int32 Row = Slot / 3;
	const int32 Col = (Slot % 3) - 1;

	FVector Location = Start->GetActorLocation() + Forward * (1800.f - Row * 900.f) + Right * (Col * 600.f);
	FVector Ground;
	if (VMUtil::GroundAt(GetWorld(), Location, Ground))
	{
		Location = Ground;
	}
	Location.Z += 120.f;
	return FTransform(Forward.Rotation(), Location);
}

void AVMGameMode::StartPlay()
{
	EnsureArena();
	Super::StartPlay();

	State = EVMMatchState::Countdown;
	TimeLeft = StartingTime;
	CountdownLeft = CountdownSeconds;

	SpawnRivals();
	for (int32 i = 0; i < MaxPedestrians; ++i)
	{
		SpawnPedestrian();
	}

	PushMessage(FString::Printf(TEXT("Win: finish %d laps, wreck every rival, or score %d points"), NumLaps, TargetPoints), FLinearColor::White, 6.f);
}

void AVMGameMode::SpawnRivals()
{
	UWorld* World = GetWorld();
	UClass* Class = RivalClass ? RivalClass.Get() : AVMVehiclePawn::StaticClass();

	for (int32 i = 0; i < NumRivals; ++i)
	{
		const FTransform T = GetGridTransform(i + 1);
		AVMVehiclePawn* Rival = World->SpawnActorDeferred<AVMVehiclePawn>(Class, T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
		if (!Rival)
		{
			continue;
		}
		Rival->bIsRival = true;
		Rival->DriverName = RivalNames[i % NumRivalPresets];
		Rival->SetBeaconColor(RivalColors[i % NumRivalPresets]);
		Rival->FinishSpawning(T);

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (AVMRivalAIController* AI = World->SpawnActor<AVMRivalAIController>(Params))
		{
			AI->Possess(Rival);
		}
		Rivals.Add(Rival);
		Progress.Add(Rival, FVMRaceProgress());
	}
}

FVector AVMGameMode::RandomGroundPoint()
{
	if (Arena)
	{
		return Arena->GetRandomGroundPoint(Rng);
	}
	// Custom level: scatter around the checkpoints.
	FVector Base = FVector::ZeroVector;
	if (Checkpoints.Num() > 0)
	{
		Base = Checkpoints[Rng.RandRange(0, Checkpoints.Num() - 1)]->GetActorLocation();
	}
	const FVector P = Base + FVector(Rng.FRandRange(-4000.f, 4000.f), Rng.FRandRange(-4000.f, 4000.f), 0.f);
	FVector Ground;
	return VMUtil::GroundAt(GetWorld(), P, Ground) ? Ground : P;
}

void AVMGameMode::SpawnPedestrian()
{
	const AVMVehiclePawn* Player = GetPlayerVehicle();
	FVector Location = RandomGroundPoint();
	// Don't pop pedestrians into existence right in front of the player.
	for (int32 Attempt = 0; Attempt < 5 && Player && FVector::Dist(Location, Player->GetActorLocation()) < 2500.f; ++Attempt)
	{
		Location = RandomGroundPoint();
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
	UClass* Class = PedestrianClass ? PedestrianClass.Get() : AVMPedestrian::StaticClass();
	if (AVMPedestrian* Ped = GetWorld()->SpawnActor<AVMPedestrian>(Class, Location + FVector(0.f, 0.f, 100.f), FRotator(0.f, Rng.FRandRange(0.f, 360.f), 0.f), Params))
	{
		Pedestrians.Add(Ped);
	}
}

void AVMGameMode::MaintainPedestrians(float DeltaSeconds)
{
	Pedestrians.RemoveAll([](const TObjectPtr<AVMPedestrian>& Ped)
	{
		return !IsValid(Ped) || Ped->IsDead();
	});

	PedestrianSpawnTimer -= DeltaSeconds;
	if (PedestrianSpawnTimer <= 0.f && Pedestrians.Num() < MaxPedestrians)
	{
		PedestrianSpawnTimer = 0.5f;
		SpawnPedestrian();
	}
}

void AVMGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	for (int32 i = Messages.Num() - 1; i >= 0; --i)
	{
		Messages[i].TimeLeft -= DeltaSeconds;
		if (Messages[i].TimeLeft <= 0.f)
		{
			Messages.RemoveAt(i);
		}
	}

	if (State == EVMMatchState::Countdown)
	{
		CountdownLeft -= DeltaSeconds;
		if (CountdownLeft <= 0.f)
		{
			CountdownLeft = 0.f;
			State = EVMMatchState::Racing;
			PushMessage(TEXT("GO!"), Good, 1.5f);
		}
	}
	else if (State == EVMMatchState::Racing)
	{
		TimeLeft -= DeltaSeconds;
		if (TimeLeft <= 0.f)
		{
			TimeLeft = 0.f;
			EndMatch(false, TEXT("Out of time"));
		}
		if (GetWorld()->GetTimeSeconds() - LastKillTime > ComboWindow)
		{
			Combo = 0;
		}
	}

	MaintainPedestrians(DeltaSeconds);

	// Light up the gate the player should head for.
	const AVMCheckpoint* Next = GetNextCheckpointFor(GetPlayerVehicle());
	const int32 NextIndex = Next ? Checkpoints.IndexOfByKey(Next) : -1;
	if (NextIndex != HighlightedCheckpoint)
	{
		HighlightedCheckpoint = NextIndex;
		for (int32 i = 0; i < Checkpoints.Num(); ++i)
		{
			Checkpoints[i]->SetHighlighted(i == NextIndex);
		}
	}
}

// ------------------------------------------------------------------------------------------
// Events
// ------------------------------------------------------------------------------------------

void AVMGameMode::OnCheckpointReached(AVMVehiclePawn* Vehicle, int32 Index)
{
	const int32 N = Checkpoints.Num();
	if (!IsRaceLive() || !Vehicle || Vehicle->IsWrecked() || N < 2)
	{
		return;
	}

	FVMRaceProgress& P = Progress.FindOrAdd(Vehicle);
	const int32 SlotIndex = Checkpoints.IndexOfByPredicate([Index](const TObjectPtr<AVMCheckpoint>& CP) { return CP->Index == Index; });
	if (SlotIndex != P.NextCheckpoint)
	{
		return;
	}

	const bool bIsPlayer = Vehicle == GetPlayerVehicle();
	if (SlotIndex == 0)
	{
		++P.LapsCompleted;
		if (P.LapsCompleted >= NumLaps)
		{
			if (bIsPlayer)
			{
				EndMatch(true, TEXT("You won the race"));
			}
			else
			{
				EndMatch(false, FString::Printf(TEXT("%s won the race"), *Vehicle->DriverName));
			}
			return;
		}
		if (bIsPlayer)
		{
			PushMessage(FString::Printf(TEXT("LAP %d / %d"), P.LapsCompleted + 1, NumLaps), Good);
		}
	}
	P.NextCheckpoint = (SlotIndex + 1) % N;

	if (bIsPlayer)
	{
		AddTime(CheckpointTimeBonus);
		AddPoints(CheckpointPoints);
		PushMessage(FString::Printf(TEXT("Checkpoint  +%.0fs"), CheckpointTimeBonus), Good);
	}
}

void AVMGameMode::OnPedestrianKilled(AVMPedestrian* Pedestrian, AVMVehiclePawn* Killer)
{
	if (!Killer || Killer != GetPlayerVehicle() || !IsRaceLive())
	{
		return;
	}

	const double Now = GetWorld()->GetTimeSeconds();
	Combo = (Now - LastKillTime <= ComboWindow) ? FMath::Min(Combo + 1, 10) : 1;
	LastKillTime = Now;
	++PedestriansKilled;

	const int32 Earned = PedestrianPoints * Combo;
	AddTime(PedestrianTimeBonus);
	AddPoints(Earned);
	PushMessage(Combo > 1
		? FString::Printf(TEXT("Pedestrian! x%d combo  +%d"), Combo, Earned)
		: FString::Printf(TEXT("Pedestrian!  +%d"), Earned), Carnage);
}

void AVMGameMode::OnVehicleDamaged(AVMVehiclePawn* Victim, AVMVehiclePawn* Attacker, float Amount)
{
	if (!IsRaceLive() || !Attacker || Attacker == Victim || Attacker != GetPlayerVehicle() || Amount <= 0.f)
	{
		return;
	}
	const int32 Earned = FMath::RoundToInt(Amount * PointsPerDamage);
	AddTime(Amount * TimePerDamage);
	AddPoints(Earned);
	if (Amount >= 10.f)
	{
		PushMessage(FString::Printf(TEXT("Smashed %s  +%d"), *Victim->DriverName, Earned), Carnage, 2.f);
	}
}

void AVMGameMode::OnVehicleWrecked(AVMVehiclePawn* Victim, AVMVehiclePawn* Attacker)
{
	if (!Victim)
	{
		return;
	}

	if (Victim == GetPlayerVehicle())
	{
		EndMatch(false, TEXT("Your car is scrap"));
		return;
	}

	++RivalsWrecked;
	if (Attacker && Attacker == GetPlayerVehicle())
	{
		AddTime(WreckTimeBonus);
		AddPoints(WreckPoints);
		PushMessage(FString::Printf(TEXT("WRECKED %s!  +%d"), *Victim->DriverName, WreckPoints), Carnage, 4.f);
	}
	else
	{
		PushMessage(FString::Printf(TEXT("%s is out"), *Victim->DriverName), FLinearColor(0.8f, 0.8f, 0.8f));
	}

	const bool bAllWrecked = !Rivals.ContainsByPredicate([](const TObjectPtr<AVMVehiclePawn>& R)
	{
		return IsValid(R) && !R->IsWrecked();
	});
	if (bAllWrecked && Rivals.Num() > 0)
	{
		EndMatch(true, TEXT("Every rival wrecked"));
	}
}

void AVMGameMode::AddTime(float Seconds)
{
	if (IsMatchOver())
	{
		return;
	}
	TimeLeft = FMath::Max(0.f, TimeLeft + Seconds);
}

void AVMGameMode::AddPoints(int32 Amount)
{
	if (IsMatchOver())
	{
		return;
	}
	Points += Amount;
	if (IsRaceLive() && Points >= TargetPoints)
	{
		EndMatch(true, FString::Printf(TEXT("Carnage target reached: %d points"), Points));
	}
}

void AVMGameMode::PushMessage(const FString& Text, const FLinearColor& Color, float Duration)
{
	FVMFeedMessage& Msg = Messages.AddDefaulted_GetRef();
	Msg.Text = Text;
	Msg.Color = Color;
	Msg.TimeLeft = Duration;
	if (Messages.Num() > 7)
	{
		Messages.RemoveAt(0);
	}
}

void AVMGameMode::EndMatch(bool bWon, const FString& Reason)
{
	if (IsMatchOver())
	{
		return;
	}
	State = bWon ? EVMMatchState::Won : EVMMatchState::Lost;
	ResultText = Reason;
	UE_LOG(LogVehicular, Log, TEXT("Match over (%s): %s"), bWon ? TEXT("won") : TEXT("lost"), *Reason);
}

void AVMGameMode::RestartMatch()
{
	const FString LevelPath = UWorld::RemovePIEPrefix(GetWorld()->GetOutermost()->GetName());
	UGameplayStatics::OpenLevel(this, FName(*LevelPath));
}

// ------------------------------------------------------------------------------------------
// Queries
// ------------------------------------------------------------------------------------------

AVMVehiclePawn* AVMGameMode::GetPlayerVehicle() const
{
	return Cast<AVMVehiclePawn>(UGameplayStatics::GetPlayerPawn(this, 0));
}

TArray<AVMVehiclePawn*> AVMGameMode::GetAllVehicles() const
{
	TArray<AVMVehiclePawn*> Result;
	if (AVMVehiclePawn* Player = GetPlayerVehicle())
	{
		Result.Add(Player);
	}
	for (const TObjectPtr<AVMVehiclePawn>& Rival : Rivals)
	{
		if (IsValid(Rival))
		{
			Result.Add(Rival);
		}
	}
	return Result;
}

FVMRaceProgress AVMGameMode::GetProgress(const AVMVehiclePawn* Vehicle) const
{
	if (const FVMRaceProgress* P = Progress.Find(const_cast<AVMVehiclePawn*>(Vehicle)))
	{
		return *P;
	}
	return FVMRaceProgress();
}

AVMCheckpoint* AVMGameMode::GetNextCheckpointFor(const AVMVehiclePawn* Vehicle) const
{
	const int32 N = Checkpoints.Num();
	if (!Vehicle || N == 0)
	{
		return nullptr;
	}
	return Checkpoints[GetProgress(Vehicle).NextCheckpoint % N];
}

AVMCheckpoint* AVMGameMode::GetCheckpointAfter(const AVMCheckpoint* Checkpoint) const
{
	const int32 N = Checkpoints.Num();
	const int32 Idx = Checkpoints.IndexOfByKey(Checkpoint);
	return (Idx == INDEX_NONE || N == 0) ? nullptr : Checkpoints[(Idx + 1) % N];
}

int32 AVMGameMode::GetLapFor(const AVMVehiclePawn* Vehicle) const
{
	return FMath::Min(GetProgress(Vehicle).LapsCompleted + 1, NumLaps);
}

float AVMGameMode::ProgressScore(const AVMVehiclePawn* Vehicle) const
{
	const int32 N = Checkpoints.Num();
	if (!Vehicle || N < 2)
	{
		return 0.f;
	}
	const FVMRaceProgress P = GetProgress(Vehicle);
	const int32 Next = P.NextCheckpoint % N;
	const int32 Prev = (Next - 1 + N) % N;
	const int32 Passed = P.LapsCompleted * N + Prev;
	const float Segment = FMath::Max(1.f, FVector::Dist(Checkpoints[Prev]->GetActorLocation(), Checkpoints[Next]->GetActorLocation()));
	const float ToNext = FVector::Dist(Vehicle->GetActorLocation(), Checkpoints[Next]->GetActorLocation());
	return Passed + FMath::Clamp(1.f - ToNext / Segment, 0.f, 1.f);
}

int32 AVMGameMode::GetRacePosition(const AVMVehiclePawn* Vehicle) const
{
	const float Mine = ProgressScore(Vehicle);
	int32 Position = 1;
	for (const AVMVehiclePawn* Other : GetAllVehicles())
	{
		if (Other != Vehicle && ProgressScore(Other) > Mine)
		{
			++Position;
		}
	}
	return Position;
}
