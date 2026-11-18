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
	Lost,
	Garage,
	Menu
};

/** The built-in arenas. Career events rotate through them in order. */
UENUM(BlueprintType)
enum class EVMArena : uint8
{
	Lot,
	SaltFlats,
	Dockyard,
	Downtown,
	Count UMETA(Hidden)
};

namespace VMArenas
{
	constexpr int32 Num = static_cast<int32>(EVMArena::Count);

	inline FString GetName(EVMArena Arena)
	{
		switch (Arena)
		{
		case EVMArena::Lot:       return TEXT("THE LOT");
		case EVMArena::SaltFlats: return TEXT("SALT FLATS");
		case EVMArena::Dockyard:  return TEXT("DOCKYARD");
		case EVMArena::Downtown:  return TEXT("DOWNTOWN");
		default:                  return TEXT("-");
		}
	}

	inline FString GetBlurb(EVMArena Arena)
	{
		switch (Arena)
		{
		case EVMArena::Lot:       return TEXT("Oval circuit around a jump plaza, with gaps to cut through");
		case EVMArena::SaltFlats: return TEXT("Wide-open figure eight with a deadly crossroads in the middle");
		case EVMArena::Dockyard:  return TEXT("Container maze ringed by a fast loop under the cranes");
		case EVMArena::Downtown:  return TEXT("City blocks, side streets and crowded sidewalks");
		default:                  return FString();
		}
	}
}

/** Procedurally synthesised sound effects (override any of them in Project Settings > Vehicular Audio). */
UENUM(BlueprintType)
enum class EVMSound : uint8
{
	ImpactLight,
	ImpactHeavy,
	Explosion,
	Squash,
	Yelp,
	Pickup,
	Checkpoint,
	CountdownBeep,
	Go,
	EMP,
	Oil,
	Nitro,
	Recover,
	Purchase,
	Denied,
	MenuMove,
	Fanfare,
	Defeat
};

/** Garage upgrades bought with credits between events. */
UENUM(BlueprintType)
enum class EVMUpgrade : uint8
{
	Engine,
	Tyres,
	Armour,
	NitroTank,
	RamPlating,
	Count UMETA(Hidden)
};

USTRUCT(BlueprintType)
struct FVMUpgradeLevels
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Upgrades")
	TArray<int32> Levels;

	int32 Get(EVMUpgrade Upgrade) const
	{
		const int32 Index = static_cast<int32>(Upgrade);
		return Levels.IsValidIndex(Index) ? Levels[Index] : 0;
	}

	void Set(EVMUpgrade Upgrade, int32 Level)
	{
		const int32 Index = static_cast<int32>(Upgrade);
		if (Levels.Num() <= Index)
		{
			Levels.SetNumZeroed(Index + 1);
		}
		Levels[Index] = Level;
	}

	static FVMUpgradeLevels Uniform(int32 Level)
	{
		FVMUpgradeLevels Result;
		Result.Levels.Init(Level, static_cast<int32>(EVMUpgrade::Count));
		return Result;
	}
};

namespace VMUpgrades
{
	constexpr int32 MaxLevel = 5;
	constexpr int32 NumUpgrades = static_cast<int32>(EVMUpgrade::Count);

	/** Price of buying the given level (1..MaxLevel). */
	inline int32 CostForLevel(int32 Level)
	{
		static const int32 Costs[MaxLevel] = { 400, 900, 1600, 2500, 3600 };
		return (Level >= 1 && Level <= MaxLevel) ? Costs[Level - 1] : -1;
	}

	inline FString GetName(EVMUpgrade Upgrade)
	{
		switch (Upgrade)
		{
		case EVMUpgrade::Engine:     return TEXT("ENGINE");
		case EVMUpgrade::Tyres:      return TEXT("TYRES");
		case EVMUpgrade::Armour:     return TEXT("ARMOUR PLATING");
		case EVMUpgrade::NitroTank:  return TEXT("NITRO TANK");
		case EVMUpgrade::RamPlating: return TEXT("RAM BAR");
		default:                     return TEXT("-");
		}
	}

	inline FString GetEffect(EVMUpgrade Upgrade)
	{
		switch (Upgrade)
		{
		case EVMUpgrade::Engine:     return TEXT("+10% engine torque per level");
		case EVMUpgrade::Tyres:      return TEXT("+6% grip per level");
		case EVMUpgrade::Armour:     return TEXT("-8% damage taken per level");
		case EVMUpgrade::NitroTank:  return TEXT("+20% boost duration, fuller starting tank");
		case EVMUpgrade::RamPlating: return TEXT("+12% ramming damage dealt per level");
		default:                     return FString();
		}
	}
}

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
