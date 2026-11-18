#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "VMPedestrian.generated.h"

class UStaticMeshComponent;
class AVMVehiclePawn;

/**
 * A simple pedestrian built from basic shapes. Wanders, panics and runs from fast cars,
 * and goes flying when hit. Swap in a skeletal mesh via a Blueprint subclass if you like.
 */
UCLASS()
class VEHICULAR_API AVMPedestrian : public ACharacter
{
	GENERATED_BODY()

public:
	AVMPedestrian(const FObjectInitializer& ObjectInitializer);
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, Category = "Pedestrian") float WalkSpeed = 150.f;
	UPROPERTY(EditAnywhere, Category = "Pedestrian") float RunSpeed = 560.f;
	UPROPERTY(EditAnywhere, Category = "Pedestrian") float PanicRadius = 2500.f;
	/** Cars slower than this (cm/s) just shove pedestrians aside. */
	UPROPERTY(EditAnywhere, Category = "Pedestrian") float LethalSpeed = 350.f;

	bool IsDead() const { return bDead; }
	void Knockdown(AVMVehiclePawn* Killer);

protected:
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UStaticMeshComponent> Body;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UStaticMeshComponent> Head;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UStaticMeshComponent> Splat;

	UFUNCTION()
	void OnCapsuleOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

private:
	AVMVehiclePawn* FindThreat() const;
	void PickWanderTarget();

	FVector WanderTarget = FVector::ZeroVector;
	FVector FleeDirection = FVector::ZeroVector;
	float ThinkTimer = 0.f;
	float PanicTime = 0.f;
	float JukeSign = 1.f;
	bool bWasPanicking = false;
	bool bDead = false;
};
