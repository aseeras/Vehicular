// Main menu, sub-screens and pause menu for AVMGameMode.

#include "VMGameMode.h"
#include "VMCarCatalog.h"
#include "VMProfileSubsystem.h"
#include "VMSoundSubsystem.h"

#include "GameFramework/GameUserSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

namespace
{
	FString Percent(float Value)
	{
		return FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Value * 100.f));
	}

	const TCHAR* QualityNames[] = { TEXT("LOW"), TEXT("MEDIUM"), TEXT("HIGH"), TEXT("EPIC") };

	const EWindowMode::Type WindowModes[] = { EWindowMode::Fullscreen, EWindowMode::WindowedFullscreen, EWindowMode::Windowed };
	const TCHAR* WindowNames[] = { TEXT("FULLSCREEN"), TEXT("BORDERLESS"), TEXT("WINDOWED") };

	int32 WindowModeIndex(const UGameUserSettings* Settings)
	{
		const EWindowMode::Type Mode = Settings ? Settings->GetFullscreenMode() : EWindowMode::Fullscreen;
		for (int32 i = 0; i < static_cast<int32>(UE_ARRAY_COUNT(WindowModes)); ++i)
		{
			if (WindowModes[i] == Mode)
			{
				return i;
			}
		}
		return 0;
	}

	int32 QualityIndex(const UGameUserSettings* Settings)
	{
		const int32 Level = Settings ? Settings->GetOverallScalabilityLevel() : 2;
		return Level < 0 ? 2 : FMath::Clamp(Level, 0, 3); // -1 means "custom"
	}

	TArray<FName> OwnedCars(const UVMProfileSubsystem* Profile)
	{
		TArray<FName> Result;
		for (const FVMCarDefinition& Car : UVMCarCatalog::GetCars())
		{
			if (Profile && Profile->IsCarOwned(Car.Id))
			{
				Result.Add(Car.Id);
			}
		}
		return Result;
	}

	FString CarName(FName Id)
	{
		const FVMCarDefinition* Car = UVMCarCatalog::Find(Id);
		return Car ? Car->DisplayName : Id.ToString();
	}

	constexpr int32 MaxQuickDifficulty = 8;
}

bool AVMGameMode::IsPauseMenuOpen() const
{
	return (State == EVMMatchState::Countdown || State == EVMMatchState::Racing) && UGameplayStatics::IsGamePaused(this);
}

void AVMGameMode::OpenMainMenu(EVMMenuScreen Screen)
{
	if (UGameplayStatics::IsGamePaused(this))
	{
		UGameplayStatics::SetGamePaused(this, false);
	}

	// When backing out of a sub-screen, land on the item that opened it.
	int32 Selection = 0;
	if (State == EVMMatchState::Menu && Screen == EVMMenuScreen::Main)
	{
		switch (MenuScreen)
		{
		case EVMMenuScreen::ConfirmNewCareer: Selection = 1; break;
		case EVMMenuScreen::QuickRace:        Selection = 2; break;
		case EVMMenuScreen::Settings:         Selection = 3; break;
		case EVMMenuScreen::Controls:         Selection = 4; break;
		default: break;
		}
	}
	else if (Screen == EVMMenuScreen::QuickRace)
	{
		Selection = 3; // "start race"
	}

	State = EVMMatchState::Menu;
	MenuScreen = Screen;
	MenuSelection = Selection;
}

void AVMGameMode::TogglePause()
{
	if (State != EVMMatchState::Countdown && State != EVMMatchState::Racing)
	{
		return;
	}
	const bool bPause = !UGameplayStatics::IsGamePaused(this);
	PauseSelection = 0;
	UGameplayStatics::SetGamePaused(this, bPause);
	UVMSoundSubsystem::Play2D(this, EVMSound::MenuMove, 0.6f);
}

void AVMGameMode::HandleBack()
{
	if (IsPauseMenuOpen())
	{
		TogglePause();
		return;
	}
	if (IsInGarage())
	{
		OpenMainMenu();
		UVMSoundSubsystem::Play2D(this, EVMSound::MenuMove, 0.6f);
		return;
	}
	if (IsInMenu() && MenuScreen != EVMMenuScreen::Main)
	{
		OpenMainMenu();
		UVMSoundSubsystem::Play2D(this, EVMSound::MenuMove, 0.6f);
	}
}

void AVMGameMode::GetMenuRows(TArray<FVMMenuRow>& OutRows) const
{
	OutRows.Reset();
	auto Row = [&OutRows](const FString& Label, const FString& Value = FString(), bool bAdjustable = false)
	{
		FVMMenuRow& R = OutRows.AddDefaulted_GetRef();
		R.Label = Label;
		R.Value = Value;
		R.bAdjustable = bAdjustable;
	};

	const UVMProfileSubsystem* Profile = UVMProfileSubsystem::Get(this);

	if (IsPauseMenuOpen())
	{
		Row(TEXT("RESUME"));
		Row(bQuickRace ? TEXT("RESTART RACE") : TEXT("RESTART EVENT"));
		Row(TEXT("QUIT TO MAIN MENU"));
		return;
	}

	switch (MenuScreen)
	{
	case EVMMenuScreen::Main:
	{
		const int32 Event = Profile ? Profile->GetEventNumber() : 1;
		Row(TEXT("CAREER"), FString::Printf(TEXT("Event %d - %s"), Event, *VMArenas::GetName(GetArenaForEvent(Event))));
		Row(TEXT("NEW CAREER"));
		Row(TEXT("QUICK RACE"));
		Row(TEXT("SETTINGS"));
		Row(TEXT("CONTROLS"));
		Row(TEXT("QUIT"));
		break;
	}
	case EVMMenuScreen::ConfirmNewCareer:
		Row(TEXT("NO, KEEP MY CAREER"));
		Row(TEXT("YES, START OVER"));
		break;
	case EVMMenuScreen::QuickRace:
		Row(TEXT("ARENA"), Profile ? VMArenas::GetName(Profile->QuickArena) : FString(), true);
		Row(TEXT("CAR"), Profile ? CarName(Profile->GetSelectedCar()) : FString(), true);
		Row(TEXT("RIVAL DIFFICULTY"), FString::Printf(TEXT("%d / %d"), Profile ? Profile->QuickDifficulty : 1, MaxQuickDifficulty), true);
		Row(TEXT("START RACE"));
		Row(TEXT("BACK"));
		break;
	case EVMMenuScreen::Settings:
	{
		const UGameUserSettings* GUS = UGameUserSettings::GetGameUserSettings();
		Row(TEXT("MASTER VOLUME"), Profile ? Percent(Profile->GetMasterVolume()) : FString(), true);
		Row(TEXT("EFFECTS VOLUME"), Profile ? Percent(Profile->GetEffectsVolume()) : FString(), true);
		Row(TEXT("ENGINE VOLUME"), Profile ? Percent(Profile->GetEngineVolume()) : FString(), true);
		Row(TEXT("PARTICLES"), Profile ? Percent(Profile->GetParticleDensity()) : FString(), true);
		Row(TEXT("GRAPHICS"), QualityNames[QualityIndex(GUS)], true);
		Row(TEXT("WINDOW"), WindowNames[WindowModeIndex(GUS)], true);
		Row(TEXT("BACK"));
		break;
	}
	case EVMMenuScreen::Controls:
		Row(TEXT("BACK"));
		break;
	}
}

void AVMGameMode::ActivateMenuRow()
{
	UVMProfileSubsystem* Profile = UVMProfileSubsystem::Get(this);
	auto Click = [this]() { UVMSoundSubsystem::Play2D(this, EVMSound::MenuMove); };

	switch (MenuScreen)
	{
	case EVMMenuScreen::Main:
		switch (MenuSelection)
		{
		case 0: Click(); OpenGarage(); break;
		case 1: Click(); OpenMainMenu(EVMMenuScreen::ConfirmNewCareer); break;
		case 2:
			Click();
			if (Profile && Profile->QuickDifficulty < 1)
			{
				Profile->QuickDifficulty = 1;
			}
			OpenMainMenu(EVMMenuScreen::QuickRace);
			break;
		case 3: Click(); OpenMainMenu(EVMMenuScreen::Settings); break;
		case 4: Click(); OpenMainMenu(EVMMenuScreen::Controls); break;
		case 5:
			UKismetSystemLibrary::QuitGame(this, UGameplayStatics::GetPlayerController(this, 0), EQuitPreference::Quit, false);
			break;
		default: break;
		}
		break;

	case EVMMenuScreen::ConfirmNewCareer:
		if (MenuSelection == 1 && Profile)
		{
			Profile->ResetCareer();
			UVMSoundSubsystem::Play2D(this, EVMSound::Denied);
			OpenMainMenu();
			MenuSelection = 0;
		}
		else
		{
			Click();
			OpenMainMenu();
		}
		break;

	case EVMMenuScreen::QuickRace:
		if (MenuSelection == 3)
		{
			if (Profile)
			{
				Profile->SetNextLoadIntent(EVMLoadIntent::QuickRace);
			}
			RestartMatch();
		}
		else if (MenuSelection == 4)
		{
			Click();
			OpenMainMenu();
		}
		else
		{
			AdjustMenuRow(1);
		}
		break;

	case EVMMenuScreen::Settings:
		if (MenuSelection == 6)
		{
			Click();
			OpenMainMenu();
		}
		else
		{
			AdjustMenuRow(1);
		}
		break;

	case EVMMenuScreen::Controls:
		Click();
		OpenMainMenu();
		break;
	}
}

void AVMGameMode::AdjustMenuRow(int32 Direction)
{
	UVMProfileSubsystem* Profile = UVMProfileSubsystem::Get(this);
	if (!Profile)
	{
		return;
	}
	bool bChanged = false;

	if (MenuScreen == EVMMenuScreen::QuickRace)
	{
		switch (MenuSelection)
		{
		case 0:
			Profile->QuickArena = static_cast<EVMArena>((static_cast<int32>(Profile->QuickArena) + Direction + VMArenas::Num) % VMArenas::Num);
			bChanged = true;
			break;
		case 1:
		{
			const TArray<FName> Cars = OwnedCars(Profile);
			if (Cars.Num() > 0)
			{
				const int32 Current = FMath::Max(0, Cars.IndexOfByKey(Profile->GetSelectedCar()));
				Profile->SelectCar(Cars[(Current + Direction + Cars.Num()) % Cars.Num()]);
				bChanged = true;
			}
			break;
		}
		case 2:
			Profile->QuickDifficulty = FMath::Clamp(Profile->QuickDifficulty + Direction, 1, MaxQuickDifficulty);
			bChanged = true;
			break;
		default: break;
		}
	}
	else if (MenuScreen == EVMMenuScreen::Settings)
	{
		const float Step = 0.1f * Direction;
		UGameUserSettings* GUS = UGameUserSettings::GetGameUserSettings();
		switch (MenuSelection)
		{
		case 0: Profile->SetMasterVolume(Profile->GetMasterVolume() + Step); bChanged = true; break;
		case 1: Profile->SetEffectsVolume(Profile->GetEffectsVolume() + Step); bChanged = true; break;
		case 2: Profile->SetEngineVolume(Profile->GetEngineVolume() + Step); bChanged = true; break;
		case 3: Profile->SetParticleDensity(Profile->GetParticleDensity() + 0.25f * Direction); bChanged = true; break;
		case 4:
			if (GUS)
			{
				GUS->SetOverallScalabilityLevel(FMath::Clamp(QualityIndex(GUS) + Direction, 0, 3));
				GUS->ApplySettings(false);
				bChanged = true;
			}
			break;
		case 5:
			if (GUS)
			{
				const int32 Count = static_cast<int32>(UE_ARRAY_COUNT(WindowModes));
				GUS->SetFullscreenMode(WindowModes[(WindowModeIndex(GUS) + Direction + Count) % Count]);
				GUS->ApplySettings(false);
				bChanged = true;
			}
			break;
		default: break;
		}
	}

	if (bChanged)
	{
		// Play the click after a volume change so you hear the new level.
		UVMSoundSubsystem::Play2D(this, EVMSound::MenuMove);
	}
}

void AVMGameMode::ActivatePauseRow()
{
	UVMProfileSubsystem* Profile = UVMProfileSubsystem::Get(this);
	switch (PauseSelection)
	{
	case 0:
		TogglePause();
		break;
	case 1:
		UGameplayStatics::SetGamePaused(this, false);
		if (Profile)
		{
			Profile->SetNextLoadIntent(bQuickRace ? EVMLoadIntent::QuickRace : EVMLoadIntent::CareerRace);
		}
		RestartMatch();
		break;
	case 2:
		UGameplayStatics::SetGamePaused(this, false);
		if (Profile)
		{
			Profile->SetNextLoadIntent(EVMLoadIntent::MainMenu);
		}
		RestartMatch();
		break;
	default:
		break;
	}
}
