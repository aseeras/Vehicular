#include "VMProfileSubsystem.h"
#include "VMSaveGame.h"
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
	Profile = Cast<UVMSaveGame>(UGameplayStatics::CreateSaveGameObject(UVMSaveGame::StaticClass()));
	Save();
}

bool UVMProfileSubsystem::ConsumeSkipGarage()
{
	const bool bResult = bSkipGarage;
	bSkipGarage = false;
	return bResult;
}
