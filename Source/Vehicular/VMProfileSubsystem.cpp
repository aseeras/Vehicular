#include "VMProfileSubsystem.h"
#include "VMSaveGame.h"
#include "VMCarCatalog.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"

static const TCHAR* ProfileSlot = TEXT("VehicularCareer");

void UVMProfileSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (UGameplayStatics::DoesSaveGameExist(ProfileSlot, 0))
	{
		Profile = Cast<UVMSaveGame>(UGameplayStatics::LoadGameFromSlot(ProfileSlot, 0));
	}
	if (!Profile)
	{
		Profile = Cast<UVMSaveGame>(UGameplayStatics::CreateSaveGameObject(UVMSaveGame::StaticClass()));
	}
	EnsureStarterCar();

	// Default split-screen cars: a different one for each player.
	const TArray<FVMCarDefinition>& Cars = UVMCarCatalog::GetCars();
	for (int32 i = 0; i < 4; ++i)
	{
		SplitCars.Add(Cars.Num() > 0 ? Cars[i % Cars.Num()].Id : NAME_None);
	}
}

void UVMProfileSubsystem::EnsureStarterCar()
{
	for (const FVMCarDefinition& Car : UVMCarCatalog::GetCars())
	{
		if (Car.Price <= 0 && Car.RequiredWins <= 0)
		{
			Profile->OwnedCars.AddUnique(Car.Id);
		}
	}
	if (!IsCarOwned(Profile->SelectedCar))
	{
		Profile->SelectedCar = UVMCarCatalog::GetStarter().Id;
	}
}

FName UVMProfileSubsystem::GetSelectedCar() const
{
	return Profile->SelectedCar;
}

bool UVMProfileSubsystem::IsCarOwned(FName CarId) const
{
	return Profile->OwnedCars.Contains(CarId);
}

bool UVMProfileSubsystem::SelectCar(FName CarId)
{
	if (!IsCarOwned(CarId))
	{
		return false;
	}
	Profile->SelectedCar = CarId;
	Save();
	return true;
}

bool UVMProfileSubsystem::TryBuyCar(FName CarId)
{
	const FVMCarDefinition* Car = UVMCarCatalog::Find(CarId);
	if (!Car || IsCarOwned(CarId) || Profile->EventsWon < Car->RequiredWins || Profile->Credits < Car->Price)
	{
		return false;
	}
	Profile->Credits -= Car->Price;
	Profile->OwnedCars.Add(CarId);
	Profile->SelectedCar = CarId;
	Save();
	return true;
}

UVMProfileSubsystem* UVMProfileSubsystem::Get(const UObject* WorldContext)
{
	const UGameInstance* GI = UGameplayStatics::GetGameInstance(WorldContext);
	return GI ? GI->GetSubsystem<UVMProfileSubsystem>() : nullptr;
}

void UVMProfileSubsystem::Save()
{
	UGameplayStatics::SaveGameToSlot(Profile, ProfileSlot, 0);
}

int32 UVMProfileSubsystem::GetCredits() const { return Profile->Credits; }
int32 UVMProfileSubsystem::GetEventNumber() const { return FMath::Max(1, Profile->EventNumber); }
int32 UVMProfileSubsystem::GetEventsWon() const { return Profile->EventsWon; }
int32 UVMProfileSubsystem::GetBestPoints() const { return Profile->BestPoints; }
FVMUpgradeLevels UVMProfileSubsystem::GetUpgrades() const { return Profile->Upgrades; }
int32 UVMProfileSubsystem::GetUpgradeLevel(EVMUpgrade Upgrade) const { return Profile->Upgrades.Get(Upgrade); }

int32 UVMProfileSubsystem::GetNextUpgradeCost(EVMUpgrade Upgrade) const
{
	return VMUpgrades::CostForLevel(GetUpgradeLevel(Upgrade) + 1);
}

bool UVMProfileSubsystem::TryPurchase(EVMUpgrade Upgrade)
{
	const int32 Cost = GetNextUpgradeCost(Upgrade);
	if (Cost < 0 || Profile->Credits < Cost)
	{
		return false;
	}
	Profile->Credits -= Cost;
	Profile->Upgrades.Set(Upgrade, GetUpgradeLevel(Upgrade) + 1);
	Save();
	return true;
}

void UVMProfileSubsystem::AwardCredits(int32 Amount)
{
	Profile->Credits = FMath::Max(0, Profile->Credits + Amount);
	Save();
}

void UVMProfileSubsystem::RecordEvent(bool bWon, int32 Points)
{
	Profile->BestPoints = FMath::Max(Profile->BestPoints, Points);
	if (bWon)
	{
		++Profile->EventsWon;
		Profile->EventNumber = GetEventNumber() + 1;
	}
	Save();
}

void UVMProfileSubsystem::ResetCareer()
{
	UVMSaveGame* Fresh = Cast<UVMSaveGame>(UGameplayStatics::CreateSaveGameObject(UVMSaveGame::StaticClass()));
	// Keep the player's settings.
	Fresh->MasterVolume = Profile->MasterVolume;
	Fresh->EffectsVolume = Profile->EffectsVolume;
	Fresh->EngineVolume = Profile->EngineVolume;
	Fresh->ParticleDensity = Profile->ParticleDensity;
	Profile = Fresh;
	EnsureStarterCar();
	Save();
}

EVMLoadIntent UVMProfileSubsystem::ConsumeLoadIntent()
{
	const EVMLoadIntent Result = LoadIntent;
	LoadIntent = EVMLoadIntent::MainMenu;
	return Result;
}

float UVMProfileSubsystem::GetMasterVolume() const { return Profile->MasterVolume; }
float UVMProfileSubsystem::GetEffectsVolume() const { return Profile->EffectsVolume; }
float UVMProfileSubsystem::GetEngineVolume() const { return Profile->EngineVolume; }
float UVMProfileSubsystem::GetParticleDensity() const { return Profile->ParticleDensity; }
void UVMProfileSubsystem::SetMasterVolume(float Value) { Profile->MasterVolume = FMath::Clamp(Value, 0.f, 1.f); Save(); }
void UVMProfileSubsystem::SetEffectsVolume(float Value) { Profile->EffectsVolume = FMath::Clamp(Value, 0.f, 1.f); Save(); }
void UVMProfileSubsystem::SetEngineVolume(float Value) { Profile->EngineVolume = FMath::Clamp(Value, 0.f, 1.f); Save(); }
void UVMProfileSubsystem::SetParticleDensity(float Value) { Profile->ParticleDensity = FMath::Clamp(Value, 0.25f, 1.5f); Save(); }
