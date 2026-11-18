#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VMTypes.h"
#include "VMPickup.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class UMaterialInstanceDynamic;

/** A floating crate that gives a power-up, then respawns as a new random one. */
UCLASS()
class VEHICULAR_API AVMPickup : public AActor
{
	GENERATED_BODY()

public:
	AVMPickup();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup")
	EVMPickupType Type = EVMPickupType::Nitro;

	/** Pick a new random type at start and on every respawn. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup")
	bool bRandomType = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup")
	float RespawnDelay = 20.f;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<USphereComponent> Trigger;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UStaticMeshComponent> Crate;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UTextRenderComponent> Label;

	UFUNCTION()
	void OnTriggerOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

private:
	void Activate();

	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> CrateMaterial;
	FTimerHandle RespawnTimer;
	FRandomStream Rng;
	float Age = 0.f;
	bool bActive = false;
};
