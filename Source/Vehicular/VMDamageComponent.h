#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VMTypes.h"
#include "VMDamageComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FVMOnDamaged, EVMDamageZone, Zone, float, Amount, AActor*, DamageCauser);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FVMOnPartBroken, EVMDamageZone, Zone, int32, Stage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVMOnWrecked, AActor*, FinalBlowCauser);

/**
 * Four-zone damage model. Each zone has its own health; the owning vehicle reads the
 * zone fractions to degrade engine power, grip and steering.
 */
UCLASS(ClassGroup = (Vehicular), meta = (BlueprintSpawnableComponent))
class VEHICULAR_API UVMDamageComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVMDamageComponent();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage")
	float MaxZoneHealth = 100.f;

	/** The car is wrecked once total health falls to this fraction. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage")
	float WreckThreshold = 0.25f;

	/** Multiplier on incoming damage (armour pickups lower it). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	float DamageTakenMultiplier = 1.f;

	UFUNCTION(BlueprintCallable, Category = "Damage")
	float ApplyZoneDamage(EVMDamageZone Zone, float Amount, AActor* DamageCauser);

	UFUNCTION(BlueprintCallable, Category = "Damage")
	float ApplySpreadDamage(float Amount, AActor* DamageCauser);

	UFUNCTION(BlueprintCallable, Category = "Damage")
	void RepairAll();

	UFUNCTION(BlueprintPure, Category = "Damage")
	float GetZoneFraction(EVMDamageZone Zone) const;

	UFUNCTION(BlueprintPure, Category = "Damage")
	float GetTotalFraction() const;

	UFUNCTION(BlueprintPure, Category = "Damage")
	bool IsWrecked() const { return bWrecked; }

	UPROPERTY(BlueprintAssignable, Category = "Damage")
	FVMOnDamaged OnDamaged;

	UPROPERTY(BlueprintAssignable, Category = "Damage")
	FVMOnPartBroken OnPartBroken;

	UPROPERTY(BlueprintAssignable, Category = "Damage")
	FVMOnWrecked OnWrecked;

private:
	float DamageZoneRaw(int32 Index, float Amount);
	void CheckBreakStage(int32 Index);

	UPROPERTY(VisibleAnywhere, Category = "Damage")
	TArray<float> ZoneHealth;

	UPROPERTY()
	TArray<int32> ZoneBreakStage;

	bool bWrecked = false;
};
