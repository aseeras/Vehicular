#include "VMGameMode.h"
#include "Vehicular.h"
#include "VMArenaBuilder.h"
#include "VMCheckpoint.h"
#include "VMDamageComponent.h"
#include "VMHUD.h"
#include "VMPedestrian.h"
#include "VMPlayerController.h"
#include "VMProfileSubsystem.h"
#include "VMCarCatalog.h"
#include "VMSoundSubsystem.h"
#include "VMRivalAIController.h"
#include "VMUtil.h"
#include "VMVehiclePawn.h"

#include "Algo/Sort.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "GameMapsSettings.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameViewportClient.h"

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
	if (!bSplitScreen)
	{
		return Super::ChoosePlayerStart_Implementation(Player);
	}

	// Split-screen: each local player gets their own slot on the grid.
	const int32 Slot = FMath::Clamp(LocalIndexOf(Player), 0, 3);
	if (const TObjectPtr<AActor>* Existing = SplitStarts.Find(Slot))
	{
		if (IsValid(*Existing))
		{
			return *Existing;
		}
	}
	const FTransform T = GetGridTransform(Slot);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	APlayerStart* Start = GetWorld()->SpawnActor<APlayerStart>(T.GetLocation(), T.Rotator(), Params);
	SplitStarts.Add(Slot, Start);
	return Start;
}

int32 AVMGameMode::LocalIndexOf(const AController* Controller) const
{
	const APlayerController* PC = Cast<APlayerController>(Controller);
	const UGameInstance* GI = GetGameInstance();
	if (!PC || !PC->GetLocalPlayer() || !GI)
	{
		return 0;
	}
	return FMath::Max(0, GI->GetLocalPlayers().IndexOfByKey(PC->GetLocalPlayer()));
}

FName AVMGameMode::HumanCarId(int32 LocalIndex) const
{
	const UVMProfileSubsystem* Profile = UVMProfileSubsystem::Get(this);
	if (!Profile)
	{
		return NAME_None;
	}
	if (bSplitScreen && Profile->SplitCars.IsValidIndex(LocalIndex))
	{
		return Profile->SplitCars[LocalIndex];
	}
	return Profile->GetSelectedCar();
}

UClass* AVMGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	// A car can name its own Blueprint (with a custom mesh); otherwise use the default pawn.
	EnsureArena();
	if (const FVMCarDefinition* Car = UVMCarCatalog::Find(HumanCarId(LocalIndexOf(InController))))
	{
		if (UClass* Custom = Car->PawnClass.LoadSynchronous())
		{
			return Custom;
		}
	}
	return Super::GetDefaultPawnClassForController_Implementation(InController);
}

APawn* AVMGameMode::SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform)
{
	UClass* PawnClass = GetDefaultPawnClassForController(NewPlayer);
	if (!PawnClass || !PawnClass->IsChildOf(AVMVehiclePawn::StaticClass()))
	{
		return Super::SpawnDefaultPawnAtTransform_Implementation(NewPlayer, SpawnTransform);
	}

	// Hand each human their car and loadout before BeginPlay runs.
	const int32 Index = LocalIndexOf(NewPlayer);
	AVMVehiclePawn* Car = GetWorld()->SpawnActorDeferred<AVMVehiclePawn>(PawnClass, SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!Car)
	{
		return nullptr;
	}
	Car->CarId = HumanCarId(Index);
	Car->bLoadoutAssigned = true;
	if (bSplitScreen)
	{
		// Party mode: everyone starts on equal footing, no garage upgrades.
		Car->Upgrades = FVMUpgradeLevels();
		Car->DriverName = FString::Printf(TEXT("P%d"), Index + 1);
		Car->bShowBeacon = true;
		Car->SetBeaconColor(GetPlayerColor(Index + 1));
	}
	else if (const UVMProfileSubsystem* Profile = UVMProfileSubsystem::Get(this))
	{
		Car->Upgrades = Profile->GetUpgrades();
	}
	Car->FinishSpawning(SpawnTransform);
	return Car;
}

void AVMGameMode::SetupLocalPlayers()
{
	UGameInstance* GI = GetGameInstance();
	if (!GI)
	{
		return;
	}
	const UVMProfileSubsystem* Profile = UVMProfileSubsystem::Get(this);

	if (bSplitScreen)
	{
		// With "P1 uses keyboard", the first gamepad drives P2 instead of P1.
		GetMutableDefault<UGameMapsSettings>()->SetSkipAssigningGamepadToPlayer1(Profile && Profile->bSplitP1Keyboard);
		if (UGameViewportClient* Viewport = GetWorld()->GetGameViewport())
		{
			Viewport->SetForceDisableSplitscreen(false);
		}
		for (int32 i = GI->GetNumLocalPlayers(); i < NumHumans; ++i)
		{
			UGameplayStatics::CreatePlayer(this, i, true);
		}
		return;
	}

	// Single player: drop any extra local players left over from a split-screen match.
	GetMutableDefault<UGameMapsSettings>()->SetSkipAssigningGamepadToPlayer1(false);
	const TArray<ULocalPlayer*> LocalPlayers = GI->GetLocalPlayers();
	for (int32 i = LocalPlayers.Num() - 1; i >= 1; --i)
	{
		if (APlayerController* PC = LocalPlayers[i]->GetPlayerController(GetWorld()))
		{
			UGameplayStatics::RemovePlayer(PC, true);
		}
		else
		{
			GI->RemoveLocalPlayer(LocalPlayers[i]);
		}
	}
}

void AVMGameMode::EnsureArena()
{
	if (bArenaReady)
	{
		return;
	}
	bArenaReady = true;
	ConfigureEvent();

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
		// Each event gets its own layout of pillars and pickups.
		Arena = World->SpawnActorDeferred<AVMArenaBuilder>(AVMArenaBuilder::StaticClass(), FTransform::Identity, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		Arena->ArenaType = CurrentArena;
		Arena->RandomSeed = bQuickRace ? FMath::Rand() : 1337 + 7919 * EventNumber;
		Arena->FinishSpawning(FTransform::Identity);
		Arena->Build();
		Checkpoints = Arena->GetCheckpoints();
		MaxPedestrians = Arena->GetPedestrianCount();
	}

	Algo::Sort(Checkpoints, [](const TObjectPtr<AVMCheckpoint>& A, const TObjectPtr<AVMCheckpoint>& B)
	{
		return A->Index < B->Index;
	});

	AVMArenaBuilder::EnsureLighting(World, CurrentArena);

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

	TimeLeft = StartingTime;
	CountdownLeft = CountdownSeconds;

	// A fresh launch opens the main menu; starting a race from a menu reloads the level into the race.
	UVMProfileSubsystem* Profile = UVMProfileSubsystem::Get(this);
	const EVMLoadIntent Intent = Profile ? Profile->ConsumeLoadIntent() : EVMLoadIntent::CareerRace;
	SetupLocalPlayers();
	if (Intent == EVMLoadIntent::MainMenu)
	{
		const bool bBackToSplit = Profile && Profile->bReturnToSplitMenu;
		if (Profile)
		{
			Profile->bReturnToSplitMenu = false;
		}
		OpenMainMenu(bBackToSplit ? EVMMenuScreen::SplitScreen : EVMMenuScreen::Main);
	}
	else
	{
		State = EVMMatchState::Countdown;
	}

	SpawnRivals();
	for (int32 i = 0; i < MaxPedestrians; ++i)
	{
		SpawnPedestrian();
	}

	if (State == EVMMatchState::Countdown)
	{
		const FString Title = bSplitScreen ? FString::Printf(TEXT("%d-PLAYER SPLIT-SCREEN"), NumHumans)
			: bQuickRace ? FString(TEXT("QUICK RACE")) : FString::Printf(TEXT("EVENT %d"), EventNumber);
		PushMessage(FString::Printf(TEXT("%s: %s - finish %d laps, wreck every rival, or score %d points"), *Title, *VMArenas::GetName(CurrentArena), NumLaps, TargetPoints), FLinearColor::White, 6.f);
	}
}

void AVMGameMode::SpawnRivals()
{
	UWorld* World = GetWorld();
	const TArray<FVMCarDefinition>& Cars = UVMCarCatalog::GetCars();

	for (int32 i = 0; i < NumRivals; ++i)
	{
		// Later events bring out better cars.
		const FVMCarDefinition* Car = Cars.Num() > 0 ? &Cars[Rng.RandRange(0, FMath::Min(EventNumber, Cars.Num() - 1))] : nullptr;
		UClass* Class = Car ? Car->PawnClass.LoadSynchronous() : nullptr;
		if (!Class)
		{
			Class = RivalClass ? RivalClass.Get() : AVMVehiclePawn::StaticClass();
		}

		const FTransform T = GetGridTransform(i + NumHumans);
		AVMVehiclePawn* Rival = World->SpawnActorDeferred<AVMVehiclePawn>(Class, T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
		if (!Rival)
		{
			continue;
		}
		Rival->bIsRival = true;
		Rival->DriverName = RivalNames[i % NumRivalPresets];
		Rival->SetBeaconColor(RivalColors[i % NumRivalPresets]);
		Rival->CarId = Car ? Car->Id : NAME_None;
		Rival->Upgrades = FVMUpgradeLevels::Uniform(FMath::Clamp(RivalTier - Rng.RandRange(0, 1), 0, VMUpgrades::MaxLevel));
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
		const int32 Second = FMath::CeilToInt(CountdownLeft);
		if (Second > 0 && Second != LastCountdownBeep)
		{
			LastCountdownBeep = Second;
			UVMSoundSubsystem::Play2D(this, EVMSound::CountdownBeep);
		}
		if (CountdownLeft <= 0.f)
		{
			CountdownLeft = 0.f;
			State = EVMMatchState::Racing;
			UVMSoundSubsystem::Play2D(this, EVMSound::Go);
			PushMessage(TEXT("GO!"), Good, 1.5f);
		}
	}
	else if (State == EVMMatchState::Racing)
	{
		if (!bSplitScreen)
		{
			TimeLeft -= DeltaSeconds;
			if (TimeLeft <= 0.f)
			{
				TimeLeft = 0.f;
				EndMatch(nullptr, TEXT("Out of time"));
			}
		}
		const double Now = GetWorld()->GetTimeSeconds();
		for (TPair<TObjectPtr<AVMVehiclePawn>, FVMPlayerStats>& Pair : HumanStats)
		{
			if (Now - Pair.Value.LastKillTime > ComboWindow)
			{
				Pair.Value.Combo = 0;
			}
		}
	}

	MaintainPedestrians(DeltaSeconds);

	// Light up the gates the human players should head for.
	int32 Mask = 0;
	for (const AVMVehiclePawn* Human : GetHumanVehicles())
	{
		const int32 NextIndex = Checkpoints.IndexOfByKey(GetNextCheckpointFor(Human));
		if (NextIndex >= 0 && NextIndex < 32)
		{
			Mask |= 1 << NextIndex;
		}
	}
	if (Mask != HighlightMask)
	{
		HighlightMask = Mask;
		for (int32 i = 0; i < Checkpoints.Num() && i < 32; ++i)
		{
			Checkpoints[i]->SetHighlighted((Mask & (1 << i)) != 0);
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

	const bool bHuman = IsHuman(Vehicle);
	if (SlotIndex == 0)
	{
		++P.LapsCompleted;
		if (P.LapsCompleted >= NumLaps)
		{
			const bool bSolo = bHuman && !bSplitScreen;
			EndMatch(Vehicle, bSolo ? FString(TEXT("You won the race")) : FString::Printf(TEXT("%s won the race"), *Vehicle->DriverName));
			return;
		}
		if (bHuman)
		{
			PushMessage(FString::Printf(TEXT("LAP %d / %d"), P.LapsCompleted + 1, NumLaps), Good, 3.f, Vehicle);
		}
	}
	P.NextCheckpoint = (SlotIndex + 1) % N;

	if (bHuman)
	{
		UVMSoundSubsystem::PlayAt(this, EVMSound::Checkpoint, Vehicle->GetActorLocation(), 0.8f);
		AddTime(CheckpointTimeBonus);
		AddPoints(Vehicle, CheckpointPoints);
		PushMessage(bSplitScreen ? FString::Printf(TEXT("Checkpoint  +%d"), CheckpointPoints) : FString::Printf(TEXT("Checkpoint  +%.0fs"), CheckpointTimeBonus), Good, 3.f, Vehicle);
	}
}

void AVMGameMode::OnPedestrianKilled(AVMPedestrian* Pedestrian, AVMVehiclePawn* Killer)
{
	if (!IsHuman(Killer) || !IsRaceLive())
	{
		return;
	}

	FVMPlayerStats& Stats = HumanStats.FindOrAdd(Killer);
	const double Now = GetWorld()->GetTimeSeconds();
	Stats.Combo = (Now - Stats.LastKillTime <= ComboWindow) ? FMath::Min(Stats.Combo + 1, 10) : 1;
	Stats.LastKillTime = Now;
	++Stats.PedestriansKilled;

	const int32 Earned = PedestrianPoints * Stats.Combo;
	AddTime(PedestrianTimeBonus);
	PushMessage(Stats.Combo > 1
		? FString::Printf(TEXT("Pedestrian! x%d combo  +%d"), Stats.Combo, Earned)
		: FString::Printf(TEXT("Pedestrian!  +%d"), Earned), Carnage, 3.f, Killer);
	AddPoints(Killer, Earned);
}

void AVMGameMode::OnVehicleDamaged(AVMVehiclePawn* Victim, AVMVehiclePawn* Attacker, float Amount)
{
	if (!IsRaceLive() || !IsHuman(Attacker) || Attacker == Victim || Amount <= 0.f)
	{
		return;
	}
	const int32 Earned = FMath::RoundToInt(Amount * PointsPerDamage);
	AddTime(Amount * TimePerDamage);
	if (Amount >= 10.f)
	{
		PushMessage(FString::Printf(TEXT("Smashed %s  +%d"), *Victim->DriverName, Earned), Carnage, 2.f, Attacker);
	}
	AddPoints(Attacker, Earned);
}

void AVMGameMode::OnVehicleWrecked(AVMVehiclePawn* Victim, AVMVehiclePawn* Attacker)
{
	if (!Victim || IsMatchOver())
	{
		return;
	}

	if (IsHuman(Attacker) && Attacker != Victim)
	{
		++HumanStats.FindOrAdd(Attacker).Wrecks;
		AddTime(WreckTimeBonus);
		PushMessage(FString::Printf(TEXT("WRECKED %s!  +%d"), *Victim->DriverName, WreckPoints), Carnage, 4.f, Attacker);
		AddPoints(Attacker, WreckPoints);
		if (IsMatchOver())
		{
			return;
		}
	}

	const bool bVictimHuman = IsHuman(Victim);
	if (bVictimHuman && !bSplitScreen)
	{
		EndMatch(nullptr, TEXT("Your car is scrap"));
		return;
	}
	PushMessage(FString::Printf(TEXT("%s is out"), *Victim->DriverName), bVictimHuman ? Bad : FLinearColor(0.8f, 0.8f, 0.8f));

	TArray<AVMVehiclePawn*> Alive = GetAllVehicles().FilterByPredicate([](const AVMVehiclePawn* V) { return !V->IsWrecked(); });

	if (!bSplitScreen)
	{
		const bool bRivalsLeft = Alive.ContainsByPredicate([this](const AVMVehiclePawn* V) { return !IsHuman(V); });
		if (!bRivalsLeft && Rivals.Num() > 0)
		{
			EndMatch(GetPlayerVehicle(), TEXT("Every rival wrecked"));
		}
		return;
	}

	// Split-screen: last car standing wins; if every human is out, nobody does.
	const TArray<AVMVehiclePawn*> AliveHumans = Alive.FilterByPredicate([this](const AVMVehiclePawn* V) { return IsHuman(V); });
	if (AliveHumans.Num() == 0)
	{
		AVMVehiclePawn* Leader = nullptr;
		for (AVMVehiclePawn* Human : GetHumanVehicles())
		{
			if (!Leader || GetStatsFor(Human).Points > GetStatsFor(Leader).Points)
			{
				Leader = Human;
			}
		}
		EndMatch(nullptr, Leader ? FString::Printf(TEXT("Every player wrecked - %s scored the most"), *Leader->DriverName) : FString(TEXT("Every player wrecked")));
	}
	else if (Alive.Num() == 1)
	{
		EndMatch(Alive[0], FString::Printf(TEXT("%s is the last car standing"), *Alive[0]->DriverName));
	}
}

void AVMGameMode::AddTime(float Seconds)
{
	// Split-screen matches have no clock.
	if (IsMatchOver() || bSplitScreen)
	{
		return;
	}
	TimeLeft = FMath::Max(0.f, TimeLeft + Seconds);
}

void AVMGameMode::AddPoints(AVMVehiclePawn* Who, int32 Amount)
{
	if (IsMatchOver() || !Who)
	{
		return;
	}
	FVMPlayerStats& Stats = HumanStats.FindOrAdd(Who);
	Stats.Points += Amount;
	if (IsRaceLive() && Stats.Points >= TargetPoints)
	{
		EndMatch(Who, bSplitScreen
			? FString::Printf(TEXT("%s hit the carnage target"), *Who->DriverName)
			: FString::Printf(TEXT("Carnage target reached: %d points"), Stats.Points));
	}
}

void AVMGameMode::PushMessage(const FString& Text, const FLinearColor& Color, float Duration, AVMVehiclePawn* Target)
{
	FVMFeedMessage& Msg = Messages.AddDefaulted_GetRef();
	Msg.Text = Text;
	Msg.Color = Color;
	Msg.TimeLeft = Duration;
	Msg.Target = Target;
	if (Messages.Num() > 12)
	{
		Messages.RemoveAt(0);
	}
}

void AVMGameMode::EndMatch(AVMVehiclePawn* InWinner, const FString& Reason)
{
	if (IsMatchOver())
	{
		return;
	}
	Winner = InWinner;
	const bool bHumanWon = IsHuman(InWinner);
	State = bHumanWon ? EVMMatchState::Won : EVMMatchState::Lost;
	ResultText = Reason;

	// Only solo career races pay credits and advance the career.
	CreditsEarned = 0;
	if (!bQuickRace && !bSplitScreen)
	{
		const int32 Points = GetStatsFor(GetPlayerVehicle()).Points;
		CreditsEarned = Points / FMath::Max(1, PointsPerCredit);
		if (bHumanWon)
		{
			CreditsEarned += WinCreditBonus + WinCreditBonusPerEvent * (EventNumber - 1);
		}
		if (UVMProfileSubsystem* Profile = UVMProfileSubsystem::Get(this))
		{
			Profile->AwardCredits(CreditsEarned);
			Profile->RecordEvent(bHumanWon, Points);
		}
	}
	UVMSoundSubsystem::Play2D(this, bHumanWon ? EVMSound::Fanfare : EVMSound::Defeat);
	UE_LOG(LogVehicular, Log, TEXT("Match over: %s"), *Reason);
}

void AVMGameMode::RestartMatch()
{
	const FString LevelPath = UWorld::RemovePIEPrefix(GetWorld()->GetOutermost()->GetName());
	UGameplayStatics::OpenLevel(this, FName(*LevelPath));
}

// ------------------------------------------------------------------------------------------
// Career and garage
// ------------------------------------------------------------------------------------------

void AVMGameMode::GetEventPreview(int32 InEvent, int32& OutRivals, int32& OutTier, int32& OutTarget) const
{
	const int32 Step = FMath::Max(1, InEvent) - 1;
	OutRivals = FMath::Min(BaseNumRivals + Step / 2, MaxRivals);
	OutTier = FMath::Min(Step, VMUpgrades::MaxLevel);
	OutTarget = BaseTargetPoints + TargetPointsPerEvent * Step;
}

void AVMGameMode::ConfigureEvent()
{
	if (bEventConfigured)
	{
		return;
	}
	bEventConfigured = true;

	BaseNumRivals = NumRivals;
	BaseTargetPoints = TargetPoints;
	if (const UVMProfileSubsystem* Profile = UVMProfileSubsystem::Get(this))
	{
		const EVMLoadIntent Intent = Profile->PeekLoadIntent();
		bQuickRace = Intent == EVMLoadIntent::QuickRace;
		bSplitScreen = Intent == EVMLoadIntent::SplitScreen;
		if (bSplitScreen)
		{
			NumHumans = FMath::Clamp(Profile->SplitPlayers, 2, 4);
			EventNumber = Profile->SplitDifficulty;
			CurrentArena = Profile->SplitArena;
		}
		else
		{
			EventNumber = bQuickRace ? Profile->QuickDifficulty : Profile->GetEventNumber();
			CurrentArena = bQuickRace ? Profile->QuickArena : GetArenaForEvent(EventNumber);
		}
	}
	GetEventPreview(EventNumber, NumRivals, RivalTier, TargetPoints);
	if (bSplitScreen)
	{
		if (const UVMProfileSubsystem* Profile = UVMProfileSubsystem::Get(this))
		{
			NumRivals = FMath::Clamp(Profile->SplitRivals, 0, MaxRivals);
		}
	}
}

void AVMGameMode::OpenGarage()
{
	State = EVMMatchState::Garage;
	GarageSelection = GetNumGarageItems() - 1; // start on "start event"
	GarageNotice.Reset();
	GarageCarIndex = 0;
	if (const UVMProfileSubsystem* Profile = UVMProfileSubsystem::Get(this))
	{
		const FName Selected = Profile->GetSelectedCar();
		GarageCarIndex = FMath::Max(0, UVMCarCatalog::GetCars().IndexOfByPredicate([Selected](const FVMCarDefinition& Car) { return Car.Id == Selected; }));
	}
	if (UGameplayStatics::IsGamePaused(this))
	{
		UGameplayStatics::SetGamePaused(this, false);
	}
}

void AVMGameMode::NavigateGarage(int32 Direction)
{
	if (IsPauseMenuOpen())
	{
		PauseSelection = (PauseSelection + Direction + 3) % 3;
		UVMSoundSubsystem::Play2D(this, EVMSound::MenuMove, 0.6f);
		return;
	}
	if (IsInMenu())
	{
		TArray<FVMMenuRow> Rows;
		GetMenuRows(Rows);
		if (Rows.Num() > 0)
		{
			MenuSelection = (MenuSelection + Direction + Rows.Num()) % Rows.Num();
			UVMSoundSubsystem::Play2D(this, EVMSound::MenuMove, 0.6f);
		}
		return;
	}
	if (!IsInGarage())
	{
		return;
	}
	const int32 Count = GetNumGarageItems();
	GarageSelection = (GarageSelection + Direction + Count) % Count;
	GarageNotice.Reset();
	UVMSoundSubsystem::Play2D(this, EVMSound::MenuMove, 0.6f);
}

void AVMGameMode::NavigateGarageSide(int32 Direction)
{
	if (IsInMenu())
	{
		AdjustMenuRow(Direction);
		return;
	}
	const int32 Count = UVMCarCatalog::GetCars().Num();
	if (!IsInGarage() || GarageSelection != GetCarRowIndex() || Count == 0)
	{
		return;
	}
	GarageCarIndex = (GarageCarIndex + Direction + Count) % Count;
	GarageNotice.Reset();
	UVMSoundSubsystem::Play2D(this, EVMSound::MenuMove, 0.6f);
}

void AVMGameMode::HandleConfirm()
{
	if (IsPauseMenuOpen())
	{
		ActivatePauseRow();
		return;
	}
	if (IsInMenu())
	{
		ActivateMenuRow();
		return;
	}
	if (IsMatchOver())
	{
		if (bSplitScreen)
		{
			// Reload to drop the extra players and get back to the split-screen setup.
			if (UVMProfileSubsystem* Profile = UVMProfileSubsystem::Get(this))
			{
				Profile->bReturnToSplitMenu = true;
				Profile->SetNextLoadIntent(EVMLoadIntent::MainMenu);
			}
			RestartMatch();
			return;
		}
		if (bQuickRace)
		{
			OpenMainMenu(EVMMenuScreen::QuickRace);
		}
		else
		{
			OpenGarage();
		}
		UVMSoundSubsystem::Play2D(this, EVMSound::MenuMove);
		return;
	}
	if (!IsInGarage())
	{
		return;
	}

	UVMProfileSubsystem* Profile = UVMProfileSubsystem::Get(this);
	if (GarageSelection < VMUpgrades::NumUpgrades)
	{
		const EVMUpgrade Upgrade = static_cast<EVMUpgrade>(GarageSelection);
		if (Profile && Profile->TryPurchase(Upgrade))
		{
			UVMSoundSubsystem::Play2D(this, EVMSound::Purchase);
			GarageNotice = FString::Printf(TEXT("%s upgraded to level %d"), *VMUpgrades::GetName(Upgrade), Profile->GetUpgradeLevel(Upgrade));
		}
		else
		{
			UVMSoundSubsystem::Play2D(this, EVMSound::Denied);
			const bool bMaxed = Profile && Profile->GetNextUpgradeCost(Upgrade) < 0;
			GarageNotice = bMaxed ? TEXT("Already at max level") : TEXT("Not enough credits");
		}
		return;
	}

	if (GarageSelection == GetCarRowIndex())
	{
		const TArray<FVMCarDefinition>& Cars = UVMCarCatalog::GetCars();
		if (!Profile || !Cars.IsValidIndex(GarageCarIndex))
		{
			return;
		}
		const FVMCarDefinition& Car = Cars[GarageCarIndex];
		if (Profile->IsCarOwned(Car.Id))
		{
			Profile->SelectCar(Car.Id);
			UVMSoundSubsystem::Play2D(this, EVMSound::MenuMove);
			GarageNotice = FString::Printf(TEXT("%s selected"), *Car.DisplayName);
		}
		else if (Profile->TryBuyCar(Car.Id))
		{
			UVMSoundSubsystem::Play2D(this, EVMSound::Purchase);
			GarageNotice = FString::Printf(TEXT("%s unlocked and selected!"), *Car.DisplayName);
		}
		else
		{
			UVMSoundSubsystem::Play2D(this, EVMSound::Denied);
			GarageNotice = Profile->GetEventsWon() < Car.RequiredWins
				? FString::Printf(TEXT("Win %d event(s) to unlock %s"), Car.RequiredWins, *Car.DisplayName)
				: TEXT("Not enough credits");
		}
		return;
	}

	// Start the event: reload the level so the arena, rivals and upgrades are rebuilt.
	if (Profile)
	{
		Profile->SetNextLoadIntent(EVMLoadIntent::CareerRace);
	}
	RestartMatch();
}

// ------------------------------------------------------------------------------------------
// Queries
// ------------------------------------------------------------------------------------------

AVMVehiclePawn* AVMGameMode::GetPlayerVehicle() const
{
	return Cast<AVMVehiclePawn>(UGameplayStatics::GetPlayerPawn(this, 0));
}

TArray<AVMVehiclePawn*> AVMGameMode::GetHumanVehicles() const
{
	TArray<AVMVehiclePawn*> Result;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (AVMVehiclePawn* Car = Cast<AVMVehiclePawn>(It->Get() ? It->Get()->GetPawn() : nullptr))
		{
			Result.Add(Car);
		}
	}
	return Result;
}

bool AVMGameMode::IsHuman(const AVMVehiclePawn* Vehicle) const
{
	return Vehicle && Vehicle->IsPlayerControlled();
}

int32 AVMGameMode::GetPlayerNumber(const AVMVehiclePawn* Vehicle) const
{
	return IsHuman(Vehicle) ? LocalIndexOf(Vehicle->GetController()) + 1 : 0;
}

AVMVehiclePawn* AVMGameMode::GetNearestHuman(const FVector& Location) const
{
	AVMVehiclePawn* Best = nullptr;
	float BestDist = TNumericLimits<float>::Max();
	for (AVMVehiclePawn* Human : GetHumanVehicles())
	{
		const float Dist = FVector::DistSquared(Human->GetActorLocation(), Location);
		if (!Human->IsWrecked() && Dist < BestDist)
		{
			BestDist = Dist;
			Best = Human;
		}
	}
	return Best;
}

FLinearColor AVMGameMode::GetPlayerColor(int32 PlayerNumber)
{
	switch (PlayerNumber)
	{
	case 1:  return FLinearColor(0.15f, 0.55f, 1.f);
	case 2:  return FLinearColor(1.f, 0.2f, 0.15f);
	case 3:  return FLinearColor(0.2f, 1.f, 0.3f);
	case 4:  return FLinearColor(1.f, 0.85f, 0.1f);
	default: return FLinearColor::White;
	}
}

FVMPlayerStats AVMGameMode::GetStatsFor(const AVMVehiclePawn* Vehicle) const
{
	if (const FVMPlayerStats* Stats = HumanStats.Find(const_cast<AVMVehiclePawn*>(Vehicle)))
	{
		return *Stats;
	}
	return FVMPlayerStats();
}

TArray<AVMVehiclePawn*> AVMGameMode::GetAllVehicles() const
{
	TArray<AVMVehiclePawn*> Result = GetHumanVehicles();
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
