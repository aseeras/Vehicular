#include "VMRivalAIController.h"
#include "VMCheckpoint.h"
#include "VMDamageComponent.h"
#include "VMGameMode.h"
#include "VMVehiclePawn.h"

AVMRivalAIController::AVMRivalAIController()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AVMRivalAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	Aggression = FMath::FRandRange(0.3f, 0.85f);
	TopThrottle = FMath::FRandRange(0.82f, 1.f);
	ModeTimer = FMath::FRandRange(0.f, 1.5f);
}

void AVMRivalAIController::ChooseMode(AVMVehiclePawn* Car, AVMVehiclePawn* Player)
{
	const float Health = Car->GetDamage()->GetTotalFraction();
	const bool bPlayerAlive = Player && !Player->IsWrecked();
	const float Dist = bPlayerAlive ? FVector::Dist(Car->GetActorLocation(), Player->GetActorLocation()) : TNumericLimits<float>::Max();

	if (Health < 0.45f && Dist < 3000.f)
	{
		Mode = EVMRivalMode::Flee;
		ModeTimer = 3.f;
	}
	else if (bPlayerAlive && Dist < 4500.f && FMath::FRand() < Aggression)
	{
		Mode = EVMRivalMode::Hunt;
		ModeTimer = 4.f;
	}
	else
	{
		Mode = EVMRivalMode::Race;
		ModeTimer = 1.5f;
	}
}

FVector AVMRivalAIController::ChooseTarget(AVMVehiclePawn* Car, AVMVehiclePawn* Player) const
{
	const AVMGameMode* GM = GetWorld()->GetAuthGameMode<AVMGameMode>();

	if (Mode == EVMRivalMode::Hunt && Player)
	{
		// Aim where the player is going to be.
		return Player->GetActorLocation() + Player->GetVelocity() * 0.6f;
	}

	// Race and Flee both follow the checkpoints (fleeing just means not chasing).
	if (GM)
	{
		if (const AVMCheckpoint* Next = GM->GetNextCheckpointFor(Car))
		{
			const FVector NextLoc = Next->GetActorLocation();
			if (FVector::Dist2D(NextLoc, Car->GetActorLocation()) < 2000.f)
			{
				if (const AVMCheckpoint* After = GM->GetCheckpointAfter(Next))
				{
					return After->GetActorLocation();
				}
			}
			return NextLoc;
		}
	}
	return Car->GetActorLocation() + Car->GetActorForwardVector() * 1000.f;
}

void AVMRivalAIController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	AVMVehiclePawn* Car = Cast<AVMVehiclePawn>(GetPawn());
	AVMGameMode* GM = GetWorld()->GetAuthGameMode<AVMGameMode>();
	if (!Car || !GM || Car->IsWrecked())
	{
		return;
	}
	if (!GM->IsRaceLive())
	{
		Car->SetDriveInput(0.f, 0.f, true);
		return;
	}

	AVMVehiclePawn* Player = GM->GetPlayerVehicle();

	// Self-righting.
	if (Car->IsUpsideDown())
	{
		UpsideDownTime += DeltaSeconds;
		if (UpsideDownTime > 3.f)
		{
			Car->Recover();
			UpsideDownTime = 0.f;
		}
	}
	else
	{
		UpsideDownTime = 0.f;
	}

	ModeTimer -= DeltaSeconds;
	if (ModeTimer <= 0.f)
	{
		ChooseMode(Car, Player);
	}

	const FVector Target = ChooseTarget(Car, Player);
	const FVector Local = Car->GetActorTransform().InverseTransformPositionNoScale(Target);
	const float Angle = FMath::RadiansToDegrees(FMath::Atan2(Local.Y, Local.X));
	const float Speed = Car->GetSpeedKmh();

	float Steer = FMath::Clamp(Angle / 35.f, -1.f, 1.f);
	float Throttle = TopThrottle;
	bool bHandbrake = false;

	if (FMath::Abs(Angle) > 45.f && Speed > 60.f)
	{
		Throttle = 0.2f;
		bHandbrake = FMath::Abs(Angle) > 70.f;
	}

	// Stuck against something: back out with opposite lock.
	if (ReverseTime > 0.f)
	{
		ReverseTime -= DeltaSeconds;
		Car->SetDriveInput(-1.f, ReverseSteer, false);
		Car->SetNitroHeld(false);
		return;
	}
	if (Throttle > 0.5f && FMath::Abs(Speed) < 5.f)
	{
		StuckTime += DeltaSeconds;
		if (StuckTime > 1.5f)
		{
			StuckTime = 0.f;
			ReverseTime = 1.5f;
			ReverseSteer = -Steer;
		}
	}
	else
	{
		StuckTime = 0.f;
	}

	// Boost on straights and when lined up on the player.
	const bool bLinedUp = FMath::Abs(Angle) < 8.f;
	bool bNitro = false;
	if (Player && Mode == EVMRivalMode::Hunt)
	{
		bNitro = bLinedUp && FVector::Dist(Car->GetActorLocation(), Player->GetActorLocation()) < 2500.f;
	}
	else
	{
		bNitro = bLinedUp && Car->GetNitroCharge() > 0.6f;
	}
	Car->SetNitroHeld(bNitro);

	// Use held weapons when they'll do something.
	if (Player && !Player->IsWrecked())
	{
		const float PlayerDist = FVector::Dist(Car->GetActorLocation(), Player->GetActorLocation());
		const FVector PlayerLocal = Car->GetActorTransform().InverseTransformPositionNoScale(Player->GetActorLocation());
		if (Car->GetHeldPickup() == EVMPickupType::EMP && PlayerDist < 2500.f)
		{
			Car->UseHeldPickup();
		}
		else if (Car->GetHeldPickup() == EVMPickupType::OilSlick && PlayerDist < 2500.f && PlayerLocal.X < 0.f)
		{
			Car->UseHeldPickup();
		}
	}

	Car->SetDriveInput(Throttle, Steer, bHandbrake);
}
