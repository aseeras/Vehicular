#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "VMCarCatalog.generated.h"

class AVMVehiclePawn;
class USkeletalMesh;
class UAnimInstance;

/** Bolt-on parts built from basic shapes so each car reads differently. */
UENUM(BlueprintType)
enum class EVMBodyKit : uint8
{
	BullBar,
	RoofRack,
	Spoiler,
	SpikeRam,
	ArmourPlates,
	SideExhausts
};

USTRUCT(BlueprintType)
struct FVMCarDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Car") FName Id;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Car") FString DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Car") FString Description;

	/** Credits to buy. 0 = owned from the start. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Unlock") int32 Price = 0;
	/** Events you must have won before it can be bought. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Unlock") int32 RequiredWins = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Handling") float MassKg = 1500.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Handling") float TorqueMultiplier = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Handling") float GripMultiplier = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Handling") float SteerMultiplier = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat") float HealthMultiplier = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat") float RamMultiplier = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat") float NitroMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Look") TArray<EVMBodyKit> BodyKits;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Look") FLinearColor KitColor = FLinearColor(0.2f, 0.2f, 0.2f);

	/** Optional Blueprint subclass with its own mesh. Empty = the default car mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Look") TSoftClassPtr<AVMVehiclePawn> PawnClass;

	// Rough 0..1 ratings for the garage screen.
	float SpeedRating() const { return FMath::Clamp(TorqueMultiplier * 1500.f / MassKg * 0.6f, 0.f, 1.f); }
	float GripRating() const { return FMath::Clamp((GripMultiplier + SteerMultiplier) * 0.38f, 0.f, 1.f); }
	float ToughnessRating() const { return FMath::Clamp(HealthMultiplier * 0.45f, 0.f, 1.f); }
	float RamRating() const { return FMath::Clamp(RamMultiplier * MassKg / 1500.f * 0.3f, 0.f, 1.f); }
};

/** Project Settings > Game > Vehicular Cars: the unlockable car roster. */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Vehicular Cars"))
class VEHICULAR_API UVMCarCatalog : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UVMCarCatalog();

	/** Car model for every car without its own Blueprint. Empty = find one automatically (prefers "SportsCar"). */
	UPROPERTY(Config, EditAnywhere, Category = "Model")
	TSoftObjectPtr<USkeletalMesh> DefaultVehicleMesh;

	/** Animation Blueprint for that model (turns the wheels). Empty = find one next to the model. */
	UPROPERTY(Config, EditAnywhere, Category = "Model")
	TSoftClassPtr<UAnimInstance> DefaultVehicleAnimClass;

	/** Ordered from starter car to top tier. */
	UPROPERTY(Config, EditAnywhere, Category = "Cars")
	TArray<FVMCarDefinition> Cars;

	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	static const TArray<FVMCarDefinition>& GetCars();
	static const FVMCarDefinition* Find(FName Id);
	static const FVMCarDefinition& GetStarter();
};
