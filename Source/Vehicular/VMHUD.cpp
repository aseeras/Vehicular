#include "VMHUD.h"
#include "VMCheckpoint.h"
#include "VMDamageComponent.h"
#include "VMGameMode.h"
#include "VMVehiclePawn.h"
#include "VMProfileSubsystem.h"
#include "VMCarCatalog.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	FLinearColor HealthColor(float Fraction)
	{
		return FLinearColor::LerpUsingHSV(FLinearColor(1.f, 0.05f, 0.05f), FLinearColor(0.1f, 1.f, 0.2f), FMath::Clamp(Fraction, 0.f, 1.f));
	}

	FString Ordinal(int32 N)
	{
		const int32 Mod100 = N % 100;
		const TCHAR* Suffix = TEXT("th");
		if (Mod100 < 11 || Mod100 > 13)
		{
			switch (N % 10)
			{
			case 1: Suffix = TEXT("st"); break;
			case 2: Suffix = TEXT("nd"); break;
			case 3: Suffix = TEXT("rd"); break;
			default: break;
			}
		}
		return FString::Printf(TEXT("%d%s"), N, Suffix);
	}
}

void AVMHUD::Label(const FString& Text, float X, float Y, const FLinearColor& Color, float Scale, bool bCenter)
{
	const float FinalScale = Scale * S;
	if (bCenter)
	{
		float W, H;
		GetTextSize(Text, W, H, Font, FinalScale);
		X -= W * 0.5f;
		Y -= H * 0.5f;
	}
	DrawText(Text, FLinearColor(0.f, 0.f, 0.f, 0.8f), X + 2.f * S, Y + 2.f * S, Font, FinalScale);
	DrawText(Text, Color, X, Y, Font, FinalScale);
}

void AVMHUD::Bar(float X, float Y, float W, float H, float Fraction, const FLinearColor& Color)
{
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), X, Y, W, H);
	DrawRect(Color, X, Y, W * FMath::Clamp(Fraction, 0.f, 1.f), H);
}

bool AVMHUD::WorldToScreen(const FVector& World, FVector2D& OutScreen) const
{
	if (!PlayerOwner || !PlayerOwner->PlayerCameraManager)
	{
		return false;
	}
	const FVector CamLoc = PlayerOwner->PlayerCameraManager->GetCameraLocation();
	const FVector CamDir = PlayerOwner->PlayerCameraManager->GetCameraRotation().Vector();
	if (FVector::DotProduct(World - CamLoc, CamDir) <= 0.f)
	{
		return false;
	}
	const FVector P = const_cast<AVMHUD*>(this)->Project(World, false);
	OutScreen = FVector2D(P.X, P.Y);
	return true;
}

void AVMHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas)
	{
		return;
	}

	// Split-screen views are smaller; scale to whichever axis shrank most.
	S = FMath::Max(0.5f, FMath::Min(Canvas->ClipX / 1920.f, Canvas->ClipY / 1080.f));
	Font = GEngine->GetLargeFont();

	AVMGameMode* GM = GetWorld()->GetAuthGameMode<AVMGameMode>();
	AVMVehiclePawn* Car = Cast<AVMVehiclePawn>(GetOwningPawn());
	if (!GM)
	{
		return;
	}
	if (GM->IsInGarage())
	{
		DrawGarage(GM);
		return;
	}
	if (GM->IsInMenu())
	{
		DrawMenu(GM, false);
		return;
	}

	if (Car)
	{
		DrawRivalTags(GM, Car);
		DrawCheckpointMarker(GM, Car);
		DrawBottomLeft(Car);
		DrawDamage(Car);
	}
	DrawTopLeft(GM, Car);
	DrawTopRight(GM, Car);
	DrawFeed(GM, Car);
	DrawCenter(GM, Car);
}

void AVMHUD::DrawTopLeft(AVMGameMode* GM, AVMVehiclePawn* Car)
{
	const float X = 40.f * S;
	const FVMPlayerStats Stats = GM->GetStatsFor(Car);
	if (GM->IsSplitScreen())
	{
		// No clock in split-screen; show the player tag in their colour instead.
		const int32 Number = GM->GetPlayerNumber(Car);
		Label(FString::Printf(TEXT("P%d"), Number), X, 30.f * S, AVMGameMode::GetPlayerColor(Number), 3.f);
	}
	else
	{
		const float TimeLeft = GM->GetTimeLeft();
		const FLinearColor TimeColor = TimeLeft < 10.f ? FLinearColor(1.f, 0.2f, 0.2f) : FLinearColor::White;
		Label(FString::Printf(TEXT("%d:%02d"), FMath::FloorToInt(TimeLeft / 60.f), FMath::FloorToInt(FMath::Fmod(TimeLeft, 60.f))), X, 30.f * S, TimeColor, 3.f);
	}
	const int32 Opponents = FMath::Max(0, GM->GetAllVehicles().Num() - 1);
	Label(FString::Printf(TEXT("POINTS  %d / %d"), Stats.Points, GM->TargetPoints), X, 110.f * S, FLinearColor(1.f, 0.8f, 0.2f), 1.4f);
	Label(FString::Printf(TEXT("Pedestrians %d   Wrecks %d / %d"), Stats.PedestriansKilled, Stats.Wrecks, Opponents), X, 150.f * S, FLinearColor(0.85f, 0.85f, 0.85f), 1.1f);
	if (Stats.Combo > 1)
	{
		Label(FString::Printf(TEXT("COMBO x%d"), Stats.Combo), X, 185.f * S, FLinearColor(1.f, 0.45f, 0.1f), 1.6f);
	}
}

void AVMHUD::DrawTopRight(AVMGameMode* GM, AVMVehiclePawn* Car)
{
	if (!Car || GM->GetNumCheckpoints() < 2)
	{
		return;
	}
	const float X = Canvas->ClipX - 330.f * S;
	const int32 NumCars = GM->GetAllVehicles().Num();
	Label(FString::Printf(TEXT("POS  %s / %d"), *Ordinal(GM->GetRacePosition(Car)), NumCars), X, 30.f * S, FLinearColor::White, 1.8f);
	Label(FString::Printf(TEXT("LAP  %d / %d"), GM->GetLapFor(Car), GM->NumLaps), X, 80.f * S, FLinearColor::White, 1.5f);
	if (const AVMCheckpoint* Next = GM->GetNextCheckpointFor(Car))
	{
		Label(FString::Printf(TEXT("NEXT  %s"), Next->Index == 0 ? TEXT("FINISH") : *FString::FromInt(Next->Index)), X, 120.f * S, FLinearColor(0.3f, 1.f, 0.35f), 1.3f);
	}
}

void AVMHUD::DrawBottomLeft(AVMVehiclePawn* Car)
{
	const float X = 40.f * S;
	const float Bottom = Canvas->ClipY;
	Label(FString::Printf(TEXT("%3d km/h"), FMath::Abs(FMath::RoundToInt(Car->GetSpeedKmh()))), X, Bottom - 200.f * S, FLinearColor::White, 2.4f);

	Label(TEXT("NITRO"), X, Bottom - 120.f * S, FLinearColor(0.4f, 0.7f, 1.f), 1.f);
	Bar(X + 90.f * S, Bottom - 115.f * S, 220.f * S, 16.f * S, Car->GetNitroCharge(), FLinearColor(0.1f, 0.5f, 1.f));

	const EVMPickupType Held = Car->GetHeldPickup();
	Label(FString::Printf(TEXT("WEAPON  %s"), *VMPickups::GetName(Held)), X, Bottom - 90.f * S,
		Held == EVMPickupType::None ? FLinearColor(0.6f, 0.6f, 0.6f) : VMPickups::GetColor(Held), 1.1f);

	FString Status;
	if (Car->GetArmourTimeLeft() > 0.f) Status += FString::Printf(TEXT("ARMOUR %.0fs   "), Car->GetArmourTimeLeft());
	if (Car->GetSpikesTimeLeft() > 0.f) Status += FString::Printf(TEXT("SPIKES %.0fs   "), Car->GetSpikesTimeLeft());
	if (Car->IsStunned()) Status += TEXT("STUNNED   ");
	if (!Status.IsEmpty())
	{
		Label(Status, X, Bottom - 55.f * S, FLinearColor(1.f, 0.9f, 0.5f), 1.f);
	}
	if (Car->IsUpsideDown() && !Car->IsWrecked())
	{
		Label(TEXT("Press R / Y to recover"), Canvas->ClipX * 0.5f, Canvas->ClipY * 0.7f, FLinearColor(1.f, 0.7f, 0.2f), 1.5f, true);
	}
}

void AVMHUD::DrawDamage(AVMVehiclePawn* Car)
{
	const UVMDamageComponent* Dmg = Car->GetDamage();
	if (!Dmg)
	{
		return;
	}
	const float W = 70.f * S;
	const float H = 140.f * S;
	const float T = 12.f * S;
	const float CX = Canvas->ClipX - 140.f * S;
	const float CY = Canvas->ClipY - 140.f * S;
	const float L = CX - W * 0.5f;
	const float Top = CY - H * 0.5f;

	DrawRect(FLinearColor(0.1f, 0.1f, 0.1f, 0.7f), L, Top, W, H);
	DrawRect(HealthColor(Dmg->GetZoneFraction(EVMDamageZone::Front)), L, Top - T - 3.f * S, W, T);
	DrawRect(HealthColor(Dmg->GetZoneFraction(EVMDamageZone::Rear)), L, Top + H + 3.f * S, W, T);
	DrawRect(HealthColor(Dmg->GetZoneFraction(EVMDamageZone::Left)), L - T - 3.f * S, Top, T, H);
	DrawRect(HealthColor(Dmg->GetZoneFraction(EVMDamageZone::Right)), L + W + 3.f * S, Top, T, H);
	Label(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Dmg->GetTotalFraction() * 100.f)), CX, CY, HealthColor(Dmg->GetTotalFraction()), 1.2f, true);
}

void AVMHUD::DrawFeed(AVMGameMode* GM, AVMVehiclePawn* Car)
{
	float Y = Canvas->ClipY * 0.32f;
	for (const FVMFeedMessage& Msg : GM->GetMessages())
	{
		// Skip messages meant for another player.
		if (Msg.Target.IsValid() && Msg.Target.Get() != Car)
		{
			continue;
		}
		FLinearColor C = Msg.Color;
		C.A = FMath::Clamp(Msg.TimeLeft, 0.f, 1.f);
		Label(Msg.Text, 40.f * S, Y, C, 1.2f);
		Y += 36.f * S;
	}
}

void AVMHUD::DrawCheckpointMarker(AVMGameMode* GM, AVMVehiclePawn* Car)
{
	const AVMCheckpoint* Next = GM->GetNextCheckpointFor(Car);
	if (!Next || !GM->IsRaceLive() || GM->GetNumCheckpoints() < 2)
	{
		return;
	}
	const FVector Target = Next->GetActorLocation() + FVector(0.f, 0.f, 700.f);
	const int32 Meters = FMath::RoundToInt(FVector::Dist(Car->GetActorLocation(), Target) / 100.f);
	FVector2D Screen;
	if (WorldToScreen(Target, Screen))
	{
		const float Margin = 60.f * S;
		Screen.X = FMath::Clamp(Screen.X, Margin, Canvas->ClipX - Margin);
		Screen.Y = FMath::Clamp(Screen.Y, Margin, Canvas->ClipY - Margin);
		Label(FString::Printf(TEXT("[ %d m ]"), Meters), Screen.X, Screen.Y, FLinearColor(0.3f, 1.f, 0.35f), 1.3f, true);
	}
	else
	{
		Label(FString::Printf(TEXT("CHECKPOINT BEHIND YOU  %d m"), Meters), Canvas->ClipX * 0.5f, Canvas->ClipY - 60.f * S, FLinearColor(0.3f, 1.f, 0.35f), 1.2f, true);
	}
}

void AVMHUD::DrawRivalTags(AVMGameMode* GM, AVMVehiclePawn* Car)
{
	// Tags over every other car: rivals and, in split-screen, the other players.
	for (AVMVehiclePawn* Rival : GM->GetAllVehicles())
	{
		if (Rival == Car || !IsValid(Rival) || FVector::Dist(Rival->GetActorLocation(), Car->GetActorLocation()) > 9000.f)
		{
			continue;
		}
		FVector2D Screen;
		if (!WorldToScreen(Rival->GetActorLocation() + FVector(0.f, 0.f, 260.f), Screen))
		{
			continue;
		}
		if (Rival->IsWrecked())
		{
			Label(FString::Printf(TEXT("%s (WRECKED)"), *Rival->DriverName), Screen.X, Screen.Y, FLinearColor(0.6f, 0.6f, 0.6f), 0.9f, true);
			continue;
		}
		const float Health = Rival->GetDamage()->GetTotalFraction();
		const int32 Number = GM->GetPlayerNumber(Rival);
		Label(Rival->DriverName, Screen.X, Screen.Y - 14.f * S, Number > 0 ? AVMGameMode::GetPlayerColor(Number) : FLinearColor::White, Number > 0 ? 1.1f : 0.9f, true);
		Bar(Screen.X - 40.f * S, Screen.Y + 4.f * S, 80.f * S, 7.f * S, Health, HealthColor(Health));
	}
}

void AVMHUD::DrawCenter(AVMGameMode* GM, AVMVehiclePawn* Car)
{
	const float CX = Canvas->ClipX * 0.5f;
	const float CY = Canvas->ClipY * 0.4f;

	if (Car && !Car->HasVehicleMesh())
	{
		Label(TEXT("No car model found - add the Vehicle content pack, or pick one in Project Settings > Vehicular Cars"), CX, Canvas->ClipY * 0.15f, FLinearColor(1.f, 0.2f, 0.2f), 1.4f, true);
	}

	switch (GM->GetMatchState())
	{
	case EVMMatchState::Countdown:
		Label(FString::FromInt(FMath::CeilToInt(GM->GetCountdownLeft())), CX, CY, FLinearColor(1.f, 0.85f, 0.2f), 6.f, true);
		break;
	case EVMMatchState::Won:
	case EVMMatchState::Lost:
		DrawResults(GM, Car);
		break;
	default:
		break;
	}

	if (GM->IsPauseMenuOpen())
	{
		DrawMenu(GM, true);
	}
}

void AVMHUD::DrawResults(AVMGameMode* GM, AVMVehiclePawn* Car)
{
	const float CX = Canvas->ClipX * 0.5f;
	const float CY = Canvas->ClipY * 0.3f;
	const FLinearColor Green(0.3f, 1.f, 0.35f);
	const FLinearColor Red(1.f, 0.2f, 0.15f);
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.45f), 0.f, CY - 70.f * S, Canvas->ClipX, 520.f * S);

	if (!GM->IsSplitScreen())
	{
		const bool bWon = GM->GetMatchState() == EVMMatchState::Won;
		const FVMPlayerStats Stats = GM->GetStatsFor(Car);
		Label(bWon ? TEXT("VICTORY") : TEXT("DEFEAT"), CX, CY, bWon ? Green : Red, 5.f, true);
		Label(GM->GetResultText(), CX, CY + 90.f * S, FLinearColor::White, 1.8f, true);
		Label(FString::Printf(TEXT("%d points   %d pedestrians   %d wrecks"), Stats.Points, Stats.PedestriansKilled, Stats.Wrecks), CX, CY + 140.f * S, FLinearColor(1.f, 0.8f, 0.2f), 1.3f, true);
		if (!GM->IsQuickRace())
		{
			Label(FString::Printf(TEXT("+%d credits"), GM->GetCreditsEarned()), CX, CY + 185.f * S, FLinearColor(0.4f, 0.9f, 1.f), 1.6f, true);
		}
		Label(GM->IsQuickRace() ? TEXT("Press Enter / A to continue") : TEXT("Press Enter / A for the garage"), CX, CY + 240.f * S, FLinearColor(0.8f, 0.8f, 0.8f), 1.2f, true);
		return;
	}

	// Split-screen: who won, then a scoreboard of every player.
	const AVMVehiclePawn* Winner = GM->GetWinner();
	const int32 WinnerNumber = GM->GetPlayerNumber(Winner);
	FString Title = TEXT("NO WINNER");
	FLinearColor TitleColor = Red;
	if (Winner && Winner == Car)
	{
		Title = TEXT("YOU WIN!");
		TitleColor = Green;
	}
	else if (WinnerNumber > 0)
	{
		Title = FString::Printf(TEXT("P%d WINS"), WinnerNumber);
		TitleColor = AVMGameMode::GetPlayerColor(WinnerNumber);
	}
	else if (Winner)
	{
		Title = FString::Printf(TEXT("%s WINS"), *Winner->DriverName.ToUpper());
	}
	Label(Title, CX, CY, TitleColor, 4.f, true);
	Label(GM->GetResultText(), CX, CY + 75.f * S, FLinearColor::White, 1.4f, true);

	TArray<AVMVehiclePawn*> Humans = GM->GetHumanVehicles();
	Humans.Sort([GM](const AVMVehiclePawn& A, const AVMVehiclePawn& B) { return GM->GetStatsFor(&A).Points > GM->GetStatsFor(&B).Points; });
	float Y = CY + 125.f * S;
	for (const AVMVehiclePawn* Human : Humans)
	{
		const FVMPlayerStats Stats = GM->GetStatsFor(Human);
		const int32 Number = GM->GetPlayerNumber(Human);
		Label(FString::Printf(TEXT("P%d   %d pts   %d pedestrians   %d wrecks%s"), Number, Stats.Points, Stats.PedestriansKilled, Stats.Wrecks, Human->IsWrecked() ? TEXT("   (wrecked)") : TEXT("")),
			CX, Y, AVMGameMode::GetPlayerColor(Number), 1.2f, true);
		Y += 38.f * S;
	}
	Label(TEXT("Press Enter / A to continue"), CX, Y + 20.f * S, FLinearColor(0.8f, 0.8f, 0.8f), 1.1f, true);
}

void AVMHUD::DrawGarage(AVMGameMode* GM)
{
	const UVMProfileSubsystem* Profile = UVMProfileSubsystem::Get(this);
	if (!Profile)
	{
		return;
	}

	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	DrawRect(FLinearColor(0.02f, 0.02f, 0.03f, 0.88f), 0.f, 0.f, W, H);

	const float Left = W * 0.5f - 520.f * S;
	const float PanelW = 1040.f * S;
	const int32 Event = Profile->GetEventNumber();

	Label(TEXT("GARAGE"), W * 0.5f, 80.f * S, FLinearColor(1.f, 0.55f, 0.1f), 4.f, true);
	Label(FString::Printf(TEXT("CREDITS  %d"), Profile->GetCredits()), Left, 145.f * S, FLinearColor(0.4f, 0.9f, 1.f), 2.f);
	Label(FString::Printf(TEXT("Events won %d   Best score %d"), Profile->GetEventsWon(), Profile->GetBestPoints()), Left + PanelW - 380.f * S, 157.f * S, FLinearColor(0.7f, 0.7f, 0.7f), 1.1f);

	float Y = 220.f * S;
	for (int32 i = 0; i < GM->GetNumGarageItems(); ++i)
	{
		const bool bCarRow = i == GM->GetCarRowIndex();
		const float RowH = (bCarRow ? 150.f : 72.f) * S;
		const bool bSelected = i == GM->GetGarageSelection();
		DrawRect(bSelected ? FLinearColor(1.f, 0.55f, 0.1f, 0.25f) : FLinearColor(1.f, 1.f, 1.f, 0.05f), Left, Y, PanelW, RowH - 8.f * S);
		if (bSelected)
		{
			DrawRect(FLinearColor(1.f, 0.55f, 0.1f), Left, Y, 6.f * S, RowH - 8.f * S);
		}

		if (bCarRow)
		{
			const TArray<FVMCarDefinition>& Cars = UVMCarCatalog::GetCars();
			if (Cars.IsValidIndex(GM->GetGarageCarIndex()))
			{
				const FVMCarDefinition& Car = Cars[GM->GetGarageCarIndex()];
				const bool bOwned = Profile->IsCarOwned(Car.Id);
				const bool bCurrent = Profile->GetSelectedCar() == Car.Id;

				Label(FString::Printf(TEXT("CAR  <  %s  >   %d/%d"), *Car.DisplayName, GM->GetGarageCarIndex() + 1, Cars.Num()), Left + 24.f * S, Y + 8.f * S, FLinearColor::White, 1.5f);
				Label(Car.Description, Left + 24.f * S, Y + 42.f * S, FLinearColor(0.65f, 0.65f, 0.65f), 0.95f);

				FString Status;
				FLinearColor StatusColor;
				if (bCurrent)
				{
					Status = TEXT("SELECTED");
					StatusColor = FLinearColor(0.3f, 1.f, 0.35f);
				}
				else if (bOwned)
				{
					Status = TEXT("OWNED - select");
					StatusColor = FLinearColor(0.8f, 0.8f, 0.8f);
				}
				else if (Profile->GetEventsWon() < Car.RequiredWins)
				{
					Status = FString::Printf(TEXT("LOCKED - win %d"), Car.RequiredWins);
					StatusColor = FLinearColor(0.6f, 0.6f, 0.6f);
				}
				else
				{
					Status = FString::Printf(TEXT("%d cr"), Car.Price);
					StatusColor = Profile->GetCredits() >= Car.Price ? FLinearColor(0.4f, 1.f, 0.45f) : FLinearColor(1.f, 0.3f, 0.25f);
				}
				Label(Status, Left + PanelW - 260.f * S, Y + 12.f * S, StatusColor, 1.3f);

				// Stat bars.
				struct FStat { const TCHAR* Name; float Value; };
				const FStat Stats[] = {
					{ TEXT("SPEED"), Car.SpeedRating() }, { TEXT("HANDLING"), Car.GripRating() },
					{ TEXT("TOUGHNESS"), Car.ToughnessRating() }, { TEXT("RAMMING"), Car.RamRating() }
				};
				for (int32 StatIndex = 0; StatIndex < static_cast<int32>(UE_ARRAY_COUNT(Stats)); ++StatIndex)
				{
					const float SX = Left + 24.f * S + StatIndex * 250.f * S;
					Label(Stats[StatIndex].Name, SX, Y + 80.f * S, FLinearColor(0.7f, 0.7f, 0.7f), 0.85f);
					Bar(SX, Y + 108.f * S, 220.f * S, 12.f * S, Stats[StatIndex].Value, FLinearColor(1.f, 0.55f, 0.1f));
				}
			}
		}
		else if (i < VMUpgrades::NumUpgrades)
		{
			const EVMUpgrade Upgrade = static_cast<EVMUpgrade>(i);
			const int32 Level = Profile->GetUpgradeLevel(Upgrade);
			const int32 Cost = Profile->GetNextUpgradeCost(Upgrade);

			Label(VMUpgrades::GetName(Upgrade), Left + 24.f * S, Y + 8.f * S, FLinearColor::White, 1.5f);
			Label(VMUpgrades::GetEffect(Upgrade), Left + 24.f * S, Y + 42.f * S, FLinearColor(0.65f, 0.65f, 0.65f), 0.95f);

			// Level pips.
			const float PipX = Left + 560.f * S;
			for (int32 Pip = 0; Pip < VMUpgrades::MaxLevel; ++Pip)
			{
				DrawRect(Pip < Level ? FLinearColor(1.f, 0.55f, 0.1f) : FLinearColor(0.25f, 0.25f, 0.25f), PipX + Pip * 34.f * S, Y + 22.f * S, 26.f * S, 26.f * S);
			}

			FString CostText = TEXT("MAXED");
			FLinearColor CostColor(0.5f, 0.5f, 0.5f);
			if (Cost >= 0)
			{
				CostText = FString::Printf(TEXT("%d cr"), Cost);
				CostColor = Profile->GetCredits() >= Cost ? FLinearColor(0.4f, 1.f, 0.45f) : FLinearColor(1.f, 0.3f, 0.25f);
			}
			Label(CostText, Left + PanelW - 170.f * S, Y + 20.f * S, CostColor, 1.4f);
		}
		else
		{
			int32 Rivals, Tier, Target;
			GM->GetEventPreview(Event, Rivals, Tier, Target);
			Label(FString::Printf(TEXT("START EVENT %d  -  %s  >"), Event, *VMArenas::GetName(AVMGameMode::GetArenaForEvent(Event))), Left + 24.f * S, Y + 8.f * S, FLinearColor(0.3f, 1.f, 0.35f), 1.6f);
			Label(FString::Printf(TEXT("%d rivals   rival upgrade tier %d   carnage target %d"), Rivals, Tier, Target), Left + 24.f * S, Y + 44.f * S, FLinearColor(0.75f, 0.75f, 0.75f), 0.95f);
		}
		Y += RowH;
	}

	if (!GM->GetGarageNotice().IsEmpty())
	{
		Label(GM->GetGarageNotice(), W * 0.5f, Y + 30.f * S, FLinearColor(1.f, 0.85f, 0.3f), 1.3f, true);
	}
	Label(TEXT("Up / Down to choose     Left / Right to browse cars     Enter / A to buy, select or start     Esc / B for menu"), W * 0.5f, H - 60.f * S, FLinearColor(0.6f, 0.6f, 0.6f), 1.1f, true);
}

void AVMHUD::DrawMenu(AVMGameMode* GM, bool bPause)
{
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	const float CX = W * 0.5f;
	const FLinearColor Accent(1.f, 0.55f, 0.1f);

	// Lighter overlay so the orbiting car shows through.
	DrawRect(FLinearColor(0.02f, 0.02f, 0.03f, bPause ? 0.7f : 0.55f), 0.f, 0.f, W, H);

	FString Title;
	FString Subtitle;
	if (bPause)
	{
		Title = TEXT("PAUSED");
		Subtitle = GM->IsQuickRace() ? TEXT("Quick race") : FString::Printf(TEXT("Event %d - %s"), GM->GetEventNumber(), *VMArenas::GetName(GM->GetCurrentArena()));
	}
	else
	{
		switch (GM->GetMenuScreen())
		{
		case EVMMenuScreen::Main:
			Title = TEXT("VEHICULAR MANSLAUGHTER");
			Subtitle = TEXT("Race. Ram. Run them down.");
			break;
		case EVMMenuScreen::ConfirmNewCareer:
			Title = TEXT("NEW CAREER");
			Subtitle = TEXT("Start over? All credits, upgrades, cars and progress will be lost.");
			break;
		case EVMMenuScreen::SplitScreen:
			Title = TEXT("SPLIT-SCREEN");
			Subtitle = TEXT("2-4 players on one screen. P1 on keyboard or gamepad, everyone else on a gamepad.");
			break;
		case EVMMenuScreen::QuickRace:
			Title = TEXT("QUICK RACE");
			Subtitle = TEXT("One race on any arena. No credits, no career progress.");
			break;
		case EVMMenuScreen::Settings:
			Title = TEXT("SETTINGS");
			break;
		case EVMMenuScreen::Controls:
			Title = TEXT("CONTROLS");
			break;
		}
	}

	const float TitleY = 140.f * S;
	Label(Title, CX, TitleY, Accent, bPause ? 3.5f : 4.5f, true);
	if (!Subtitle.IsEmpty())
	{
		Label(Subtitle, CX, TitleY + 75.f * S, FLinearColor(0.8f, 0.8f, 0.8f), 1.2f, true);
	}

	float Y = 300.f * S;
	if (!bPause && GM->GetMenuScreen() == EVMMenuScreen::Controls)
	{
		DrawControls(CX - 420.f * S, Y);
		Y = H - 200.f * S;
	}
	if (!bPause && GM->GetMenuScreen() == EVMMenuScreen::QuickRace)
	{
		if (const UVMProfileSubsystem* Profile = UVMProfileSubsystem::Get(this))
		{
			Label(VMArenas::GetBlurb(Profile->QuickArena), CX, Y - 30.f * S, FLinearColor(0.6f, 0.6f, 0.6f), 1.f, true);
		}
	}

	TArray<FVMMenuRow> Rows;
	GM->GetMenuRows(Rows);
	const float RowW = 700.f * S;
	const float RowH = 64.f * S;
	const float Left = CX - RowW * 0.5f;
	for (int32 i = 0; i < Rows.Num(); ++i)
	{
		const bool bSelected = i == GM->GetMenuSelection();
		DrawRect(bSelected ? FLinearColor(1.f, 0.55f, 0.1f, 0.3f) : FLinearColor(1.f, 1.f, 1.f, 0.06f), Left, Y, RowW, RowH - 8.f * S);
		if (bSelected)
		{
			DrawRect(Accent, Left, Y, 6.f * S, RowH - 8.f * S);
		}
		Label(Rows[i].Label, Left + 28.f * S, Y + 14.f * S, bSelected ? FLinearColor::White : FLinearColor(0.8f, 0.8f, 0.8f), 1.4f);
		if (!Rows[i].Value.IsEmpty())
		{
			const FString Value = Rows[i].bAdjustable ? FString::Printf(TEXT("<  %s  >"), *Rows[i].Value) : Rows[i].Value;
			float TW, TH;
			GetTextSize(Value, TW, TH, Font, 1.2f * S);
			Label(Value, Left + RowW - TW - 28.f * S, Y + 17.f * S, bSelected ? Accent : FLinearColor(0.65f, 0.65f, 0.65f), 1.2f);
		}
		Y += RowH;
	}

	const TCHAR* Hint = bPause
		? TEXT("Up / Down to choose     Enter / A to select     P / Start / Esc to resume")
		: TEXT("Up / Down to choose     Left / Right to change     Enter / A to select     Esc / B to go back");
	Label(Hint, CX, H - 60.f * S, FLinearColor(0.6f, 0.6f, 0.6f), 1.05f, true);
}

void AVMHUD::DrawControls(float X, float Y)
{
	struct FLine { const TCHAR* Action; const TCHAR* Keys; const TCHAR* Pad; };
	static const FLine Lines[] = {
		{ TEXT("Throttle / reverse"), TEXT("W / S"), TEXT("RT / LT") },
		{ TEXT("Steer"), TEXT("A / D"), TEXT("Left stick") },
		{ TEXT("Handbrake"), TEXT("Space"), TEXT("B") },
		{ TEXT("Nitro"), TEXT("Left Shift"), TEXT("A") },
		{ TEXT("Use weapon"), TEXT("E"), TEXT("X") },
		{ TEXT("Recover"), TEXT("R"), TEXT("Y") },
		{ TEXT("Camera"), TEXT("C"), TEXT("Right stick click") },
		{ TEXT("Pause"), TEXT("P"), TEXT("Start") },
	};
	const FLinearColor Head(1.f, 0.55f, 0.1f);
	Label(TEXT("ACTION"), X, Y, Head, 1.1f);
	Label(TEXT("KEYBOARD"), X + 380.f * S, Y, Head, 1.1f);
	Label(TEXT("GAMEPAD"), X + 640.f * S, Y, Head, 1.1f);
	Y += 44.f * S;
	for (const FLine& Line : Lines)
	{
		Label(Line.Action, X, Y, FLinearColor::White, 1.15f);
		Label(Line.Keys, X + 380.f * S, Y, FLinearColor(0.8f, 0.8f, 0.8f), 1.15f);
		Label(Line.Pad, X + 640.f * S, Y, FLinearColor(0.8f, 0.8f, 0.8f), 1.15f);
		Y += 40.f * S;
	}
}
