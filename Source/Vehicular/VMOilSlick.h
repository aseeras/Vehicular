#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VMOilSlick.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

/** A puddle dropped behind a car. Anyone driving through it loses grip for a moment. */
UCLASS()
class VEHICULAR_API AVMOilSlick : public AActor
{
	GENERATED_BODY()

public:
	AVMOilSlick();
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, Category = "Oil")
	float SlipSeconds = 2.5f;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UBoxComponent> Trigger;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UStaticMeshComponent> Puddle;

	UFUNCTION()
	void OnTriggerOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};
