#pragma once

#include "CoreMinimal.h"
#include "VMTypes.generated.h"

UENUM(BlueprintType)
enum class EVMDamageZone : uint8
{
	Front,
	Rear,
	Left,
	Right
};

UENUM(BlueprintType)
enum class EVMPickupType : uint8
{
	None,
	Nitro,
	Repair,
	Armour,
	RamSpikes,
	EMP,
	OilSlick
};

UENUM(BlueprintType)
enum class EVMMatchState : uint8
{
	Countdown,
	Racing,
	Won,
	Lost
};

namespace VMPickups
{
	inline FString GetName(EVMPickupType Type)
	{
		switch (Type)
		{
		case EVMPickupType::Nitro:     return TEXT("NITRO");
		case EVMPickupType::Repair:    return TEXT("REPAIR");
		case EVMPickupType::Armour:    return TEXT("ARMOUR");
		case EVMPickupType::RamSpikes: return TEXT("RAM SPIKES");
		case EVMPickupType::EMP:       return TEXT("EMP");
		case EVMPickupType::OilSlick:  return TEXT("OIL SLICK");
		default:                       return TEXT("-");
		}
	}

	inline FLinearColor GetColor(EVMPickupType Type)
	{
		switch (Type)
		{
		case EVMPickupType::Nitro:     return FLinearColor(0.1f, 0.5f, 1.f);
		case EVMPickupType::Repair:    return FLinearColor(0.1f, 1.f, 0.2f);
		case EVMPickupType::Armour:    return FLinearColor(0.7f, 0.7f, 0.8f);
		case EVMPickupType::RamSpikes: return FLinearColor(1.f, 0.15f, 0.05f);
		case EVMPickupType::EMP:       return FLinearColor(0.8f, 0.2f, 1.f);
		case EVMPickupType::OilSlick:  return FLinearColor(0.15f, 0.15f, 0.15f);
		default:                       return FLinearColor::White;
		}
	}

	inline EVMPickupType Random(FRandomStream& Rng)
	{
		return static_cast<EVMPickupType>(Rng.RandRange(1, 6));
	}
}
