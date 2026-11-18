#include "VMDamageComponent.h"

static constexpr int32 NumZones = 4;

UVMDamageComponent::UVMDamageComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	ZoneHealth.Init(MaxZoneHealth, NumZones);
	ZoneBreakStage.Init(0, NumZones);
}

float UVMDamageComponent::DamageZoneRaw(int32 Index, float Amount)
{
	const float Applied = FMath::Min(ZoneHealth[Index], Amount);
	ZoneHealth[Index] -= Applied;
	CheckBreakStage(Index);
	return Applied;
}

void UVMDamageComponent::CheckBreakStage(int32 Index)
{
	const float Fraction = ZoneHealth[Index] / MaxZoneHealth;
	const int32 Stage = Fraction <= 0.f ? 2 : (Fraction <= 0.5f ? 1 : 0);
	if (Stage > ZoneBreakStage[Index])
	{
		ZoneBreakStage[Index] = Stage;
		OnPartBroken.Broadcast(static_cast<EVMDamageZone>(Index), Stage);
	}
}

float UVMDamageComponent::ApplyZoneDamage(EVMDamageZone Zone, float Amount, AActor* DamageCauser)
{
	if (bWrecked || Amount <= 0.f)
	{
		return 0.f;
	}

	const int32 Index = static_cast<int32>(Zone);
	const float Scaled = Amount * DamageTakenMultiplier;
	float Applied = DamageZoneRaw(Index, Scaled);

	// A zone that is already destroyed passes half of the extra damage to the rest of the car.
	const float Overflow = Scaled - Applied;
	if (Overflow > 0.f)
	{
		for (int32 i = 0; i < NumZones; ++i)
		{
			if (i != Index)
			{
				Applied += DamageZoneRaw(i, Overflow * 0.5f / (NumZones - 1));
			}
		}
	}

	OnDamaged.Broadcast(Zone, Applied, DamageCauser);

	if (GetTotalFraction() <= WreckThreshold)
	{
		bWrecked = true;
		OnWrecked.Broadcast(DamageCauser);
	}
	return Applied;
}

float UVMDamageComponent::ApplySpreadDamage(float Amount, AActor* DamageCauser)
{
	float Total = 0.f;
	for (int32 i = 0; i < NumZones && !bWrecked; ++i)
	{
		Total += ApplyZoneDamage(static_cast<EVMDamageZone>(i), Amount / NumZones, DamageCauser);
	}
	return Total;
}

void UVMDamageComponent::RepairAll()
{
	if (bWrecked)
	{
		return;
	}
	for (int32 i = 0; i < NumZones; ++i)
	{
		ZoneHealth[i] = MaxZoneHealth;
		ZoneBreakStage[i] = 0;
	}
	OnDamaged.Broadcast(EVMDamageZone::Front, 0.f, nullptr);
}

void UVMDamageComponent::SetMaxZoneHealth(float NewMax)
{
	MaxZoneHealth = FMath::Max(1.f, NewMax);
	ZoneHealth.Init(MaxZoneHealth, NumZones);
	ZoneBreakStage.Init(0, NumZones);
	bWrecked = false;
}

float UVMDamageComponent::GetZoneFraction(EVMDamageZone Zone) const
{
	return ZoneHealth[static_cast<int32>(Zone)] / MaxZoneHealth;
}

float UVMDamageComponent::GetTotalFraction() const
{
	float Sum = 0.f;
	for (float H : ZoneHealth)
	{
		Sum += H;
	}
	return Sum / (MaxZoneHealth * NumZones);
}
