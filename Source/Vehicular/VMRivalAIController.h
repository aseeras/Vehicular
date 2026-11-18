#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "VMRivalAIController.generated.h"

class AVMVehiclePawn;

UENUM()
enum class EVMRivalMode : uint8
{
	Race,
	Hunt,
	Flee
};

/** Drives a rival car: races the checkpoints, hunts the player when close, backs off when badly damaged. */
UCLASS()
class VEHICULAR_API AVMRivalAIController : public AAIController
{
	GENERATED_BODY()

public:
	AVMRivalAIController();
	virtual void OnPossess(APawn* InPawn) override;
	virtual void Tick(float DeltaSeconds) override;

	EVMRivalMode GetMode() const { return Mode; }

private:
	void ChooseMode(AVMVehiclePawn* Car, AVMVehiclePawn* Player);
	FVector ChooseTarget(AVMVehiclePawn* Car, AVMVehiclePawn* Player) const;

	EVMRivalMode Mode = EVMRivalMode::Race;
	float ModeTimer = 0.f;
	float StuckTime = 0.f;
	float ReverseTime = 0.f;
	float UpsideDownTime = 0.f;
	float ReverseSteer = 0.f;

	// Per-driver personality.
	float Aggression = 0.5f;
	float TopThrottle = 1.f;
};
