#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VMCheckpoint.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class UMaterialInstanceDynamic;

/** A race gate. Checkpoints are driven through in Index order; index 0 is the start/finish line. */
UCLASS()
class VEHICULAR_API AVMCheckpoint : public AActor
{
	GENERATED_BODY()

public:
	AVMCheckpoint();

	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Checkpoint")
	int32 Index = 0;

	/** Half the width of the gate in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Checkpoint")
	float GateHalfWidth = 2500.f;

	void SetHighlighted(bool bHighlighted);

protected:
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UBoxComponent> Trigger;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UStaticMeshComponent> PostLeft;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UStaticMeshComponent> PostRight;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UStaticMeshComponent> Banner;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UTextRenderComponent> Label;

	UFUNCTION()
	void OnTriggerOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

private:
	void LayoutGate();

	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> Materials;
	bool bIsHighlighted = false;
};
