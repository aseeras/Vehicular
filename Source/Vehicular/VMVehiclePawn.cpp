#include "VMVehiclePawn.h"
#include "Vehicular.h"
#include "VMDamageComponent.h"
#include "VMGameMode.h"
#include "VMOilSlick.h"
#include "VMProfileSubsystem.h"
#include "VMSoundSubsystem.h"
#include "VMAudioSettings.h"
#include "VMCarCatalog.h"
#include "VMEffectsSubsystem.h"
#include "VMRivalAIController.h"
#include "VMUtil.h"
#include "VMWheels.h"

#include "ChaosWheeledVehicleMovementComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Engine/CollisionProfile.h"
#include "Components/AudioComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/LocalPlayer.h"
#include "Animation/AnimInstance.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"

// Default car from Unreal's free "Vehicle" feature pack. Override in a Blueprint subclass to use your own.
static const TCHAR* DefaultMeshPath = TEXT("/Game/Vehicles/SportsCar/SKM_SportsCar.SKM_SportsCar");
static const TCHAR* DefaultAnimPath = TEXT("/Game/Vehicles/SportsCar/ABP_SportsCar.ABP_SportsCar_C");

AVMVehiclePawn::AVMVehiclePawn(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;

	USkeletalMeshComponent* VehicleMesh = GetMesh();
	if (USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, DefaultMeshPath, nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		VehicleMesh->SetSkeletalMeshAsset(Mesh);
	}
	if (UClass* AnimClass = LoadClass<UAnimInstance>(nullptr, DefaultAnimPath, nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		VehicleMesh->SetAnimInstanceClass(AnimClass);
	}
	VehicleMesh->SetSimulatePhysics(true);
	VehicleMesh->SetCollisionProfileName(UCollisionProfile::Vehicle_ProfileName);
	VehicleMesh->SetNotifyRigidBodyCollision(true);
	VehicleMesh->SetGenerateOverlapEvents(true);

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(VehicleMesh);
	SpringArm->SetRelativeLocation(FVector(0.f, 0.f, 140.f));
	SpringArm->SetRelativeRotation(FRotator(-12.f, 0.f, 0.f));
	SpringArm->TargetArmLength = 650.f;
	SpringArm->bInheritPitch = false;
	SpringArm->bInheritRoll = false;
	SpringArm->bEnableCameraLag = true;
	SpringArm->CameraLagSpeed = 6.f;
	SpringArm->bEnableCameraRotationLag = true;
	SpringArm->CameraRotationLagSpeed = 5.f;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;
	Camera->FieldOfView = 90.f;

	Beacon = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Beacon"));
	Beacon->SetupAttachment(VehicleMesh);
	Beacon->SetStaticMesh(VMUtil::Shape(TEXT("Sphere")));
	Beacon->SetRelativeLocation(FVector(0.f, 0.f, 170.f));
	Beacon->SetRelativeScale3D(FVector(0.35f));
	Beacon->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Beacon->SetCastShadow(false);

	Damage = CreateDefaultSubobject<UVMDamageComponent>(TEXT("Damage"));
	DebrisMesh = VMUtil::Shape(TEXT("Cube"));

	UChaosWheeledVehicleMovementComponent* Move = CastChecked<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent());
	Move->ChassisHeight = 144.f;
	Move->DragCoefficient = 0.31f;
	Move->DownforceCoefficient = 0.3f;
	Move->Mass = 1500.f;

	Move->WheelSetups.SetNum(4);
	Move->WheelSetups[0].WheelClass = UVMWheelFront::StaticClass();
	Move->WheelSetups[0].BoneName = FName(TEXT("Phys_Wheel_FL"));
	Move->WheelSetups[1].WheelClass = UVMWheelFront::StaticClass();
	Move->WheelSetups[1].BoneName = FName(TEXT("Phys_Wheel_FR"));
	Move->WheelSetups[2].WheelClass = UVMWheelRear::StaticClass();
	Move->WheelSetups[2].BoneName = FName(TEXT("Phys_Wheel_BL"));
	Move->WheelSetups[3].WheelClass = UVMWheelRear::StaticClass();
	Move->WheelSetups[3].BoneName = FName(TEXT("Phys_Wheel_BR"));

	Move->EngineSetup.MaxTorque = 750.f;
	Move->EngineSetup.MaxRPM = 7000.f;
	Move->EngineSetup.EngineIdleRPM = 900.f;
	Move->EngineSetup.EngineBrakeEffect = 0.2f;
	Move->EngineSetup.EngineRevUpMOI = 5.f;
	Move->EngineSetup.EngineRevDownRate = 600.f;
	if (FRichCurve* Torque = Move->EngineSetup.TorqueCurve.GetRichCurve())
	{
		Torque->Reset();
		Torque->AddKey(0.f, 0.4f);
		Torque->AddKey(2000.f, 0.8f);
		Torque->AddKey(5000.f, 1.f);
		Torque->AddKey(7000.f, 0.8f);
	}

	Move->DifferentialSetup.DifferentialType = EVehicleDifferential::AllWheelDrive;
	Move->DifferentialSetup.FrontRearSplit = 0.5f;

	Move->TransmissionSetup.bUseAutomaticGears = true;
	Move->TransmissionSetup.bUseAutoReverse = true;
	Move->TransmissionSetup.ChangeUpRPM = 6000.f;
	Move->TransmissionSetup.ChangeDownRPM = 2000.f;
	Move->TransmissionSetup.GearChangeTime = 0.2f;

	Move->SteeringSetup.SteeringType = ESteeringType::AngleRatio;
	Move->SteeringSetup.AngleRatio = 0.7f;

	AIControllerClass = AVMRivalAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::Disabled; // the game mode spawns rival controllers itself
}

void AVMVehiclePawn::BeginPlay()
{
	Super::BeginPlay();

	GetMesh()->OnComponentHit.AddDynamic(this, &AVMVehiclePawn::OnChassisHit);
	Damage->OnDamaged.AddDynamic(this, &AVMVehiclePawn::OnDamaged);
	Damage->OnPartBroken.AddDynamic(this, &AVMVehiclePawn::OnPartBroken);
	Damage->OnWrecked.AddDynamic(this, &AVMVehiclePawn::OnWrecked);

	if (!bIsRival)
	{
		if (const UVMProfileSubsystem* Profile = UVMProfileSubsystem::Get(this))
		{
			CarId = Profile->GetSelectedCar();
		}
	}
	if (UChaosWheeledVehicleMovementComponent* Move = GetChaosMovement())
	{
		BaseMaxTorque = Move->EngineSetup.MaxTorque;
	}
	BaseFrontFriction = GetDefault<UVMWheelFront>()->FrictionForceMultiplier;
	BaseRearFriction = GetDefault<UVMWheelRear>()->FrictionForceMultiplier;
	BaseSteerAngle = GetDefault<UVMWheelFront>()->MaxSteerAngle;
	ApplyCarDefinition();

	if (!bIsRival)
	{
		if (const UVMProfileSubsystem* Profile = UVMProfileSubsystem::Get(this))
		{
			Upgrades = Profile->GetUpgrades();
		}
	}
	ApplyUpgrades();

	if (UVMSoundSubsystem* Sound = UVMSoundSubsystem::Get(this))
	{
		EngineAudio = Sound->CreateEngineLoop(GetMesh());
	}

	Beacon->SetVisibility(bIsRival);
	VMUtil::Paint(Beacon, BeaconColor);

	if (!HasVehicleMesh())
	{
		UE_LOG(LogVehicular, Error, TEXT("%s has no vehicle mesh. Add the Vehicle feature pack (see README) or set a mesh in a Blueprint subclass."), *GetName());
	}
}

UChaosWheeledVehicleMovementComponent* AVMVehiclePawn::GetChaosMovement() const
{
	return Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent());
}

AVMGameMode* AVMVehiclePawn::GetVMGameMode() const
{
	return GetWorld() ? GetWorld()->GetAuthGameMode<AVMGameMode>() : nullptr;
}

bool AVMVehiclePawn::HasVehicleMesh() const
{
	return GetMesh() && GetMesh()->GetSkeletalMeshAsset() != nullptr;
}

void AVMVehiclePawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (EngineAudio)
	{
		EngineAudio->Stop();
	}
	Super::EndPlay(EndPlayReason);
}

void AVMVehiclePawn::ApplyUpgrades()
{
	const int32 EngineLvl = Upgrades.Get(EVMUpgrade::Engine);
	const int32 TyreLvl = Upgrades.Get(EVMUpgrade::Tyres);
	const int32 ArmourLvl = Upgrades.Get(EVMUpgrade::Armour);
	const int32 NitroLvl = Upgrades.Get(EVMUpgrade::NitroTank);
	const int32 RamLvl = Upgrades.Get(EVMUpgrade::RamPlating);

	BaseMaxTorque *= 1.f + 0.10f * EngineLvl;
	BaseFrontFriction *= 1.f + 0.06f * TyreLvl;
	BaseRearFriction *= 1.f + 0.06f * TyreLvl;
	ArmourMultiplier = 1.f - 0.08f * ArmourLvl;
	NitroDuration *= 1.f + 0.2f * NitroLvl;
	NitroCharge = FMath::Min(1.f, 0.5f + 0.1f * NitroLvl);
	RamMultiplier *= 1.f + 0.12f * RamLvl;
	bHandlingDirty = true;
}

void AVMVehiclePawn::UpdateEngineSound(float DeltaSeconds)
{
	const UChaosWheeledVehicleMovementComponent* Move = GetChaosMovement();
	if (!EngineAudio || !Move)
	{
		return;
	}
	const float MaxRPM = FMath::Max(1.f, Move->GetEngineMaxRotationSpeed());
	const float RPMFraction = FMath::Clamp(Move->GetEngineRotationSpeed() / MaxRPM, 0.f, 1.f);
	EngineAudio->SetPitchMultiplier(0.6f + 1.4f * RPMFraction + (bWasBoosting ? 0.15f : 0.f));

	const float TargetVolume = IsWrecked() ? 0.f : 0.35f + 0.45f * FMath::Abs(Move->GetThrottleInput()) + (bWasBoosting ? 0.2f : 0.f);
	EngineVolume = FMath::FInterpTo(EngineVolume, TargetVolume, DeltaSeconds, 6.f);
	float UserVolume = 1.f;
	if (const UVMProfileSubsystem* Profile = UVMProfileSubsystem::Get(this))
	{
		UserVolume = Profile->GetMasterVolume() * Profile->GetEngineVolume();
	}
	// Quieter engines behind the menus.
	const AVMGameMode* GM = GetVMGameMode();
	if (GM && (GM->IsInMenu() || GM->IsInGarage()))
	{
		UserVolume *= 0.3f;
	}
	EngineAudio->SetVolumeMultiplier(EngineVolume * UserVolume * GetDefault<UVMAudioSettings>()->EngineVolume);
}

bool AVMVehiclePawn::IsWrecked() const
{
	return Damage && Damage->IsWrecked();
}

bool AVMVehiclePawn::IsUpsideDown() const
{
	return GetActorUpVector().Z < 0.2f;
}

float AVMVehiclePawn::GetSpeedKmh() const
{
	const UChaosWheeledVehicleMovementComponent* Move = GetChaosMovement();
	return Move ? Move->GetForwardSpeed() * 0.036f : 0.f;
}

bool AVMVehiclePawn::CanDrive() const
{
	if (IsWrecked())
	{
		return false;
	}
	const AVMGameMode* GM = GetVMGameMode();
	return !GM || GM->IsRaceLive();
}

void AVMVehiclePawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	ArmourTime = FMath::Max(0.f, ArmourTime - DeltaSeconds);
	SpikesTime = FMath::Max(0.f, SpikesTime - DeltaSeconds);
	StunTime = FMath::Max(0.f, StunTime - DeltaSeconds);
	RecoverCooldown = FMath::Max(0.f, RecoverCooldown - DeltaSeconds);
	if (SlipTime > 0.f)
	{
		SlipTime -= DeltaSeconds;
		if (SlipTime <= 0.f)
		{
			SlipTime = 0.f;
			bHandlingDirty = true;
		}
	}
	Damage->DamageTakenMultiplier = ArmourMultiplier * (ArmourTime > 0.f ? 0.4f : 1.f);

	if (IsPlayerControlled())
	{
		SetDriveInput(PlayerThrottle, PlayerSteer, bPlayerHandbrake);
	}

	const bool bBoosting = bNitroHeld && NitroCharge > 0.f && CanDrive() && !IsStunned();
	if (bBoosting)
	{
		GetMesh()->AddForce(GetActorForwardVector() * NitroAcceleration, NAME_None, /*bAccelChange*/ true);
		NitroCharge = FMath::Max(0.f, NitroCharge - DeltaSeconds / NitroDuration);
		if (!bWasBoosting)
		{
			UVMSoundSubsystem::PlayAt(this, EVMSound::Nitro, GetActorLocation(), 0.8f);
		}
	}
	bWasBoosting = bBoosting;
	UpdateEngineSound(DeltaSeconds);
	UpdateEffects(DeltaSeconds);

	// Slow showroom orbit behind the menus and garage.
	if (!bIsRival)
	{
		const AVMGameMode* GM = GetVMGameMode();
		if (GM && (GM->IsInMenu() || GM->IsInGarage()))
		{
			MenuOrbitYaw = FMath::Fmod(MenuOrbitYaw + 8.f * DeltaSeconds, 360.f);
			SpringArm->SetRelativeRotation(FRotator(-15.f, MenuOrbitYaw + 150.f, 0.f));
			SpringArm->TargetArmLength = 800.f;
		}
	}

	if (bHandlingDirty)
	{
		UpdateHandling();
	}

	LastFrameVelocity = GetVelocity();
}

void AVMVehiclePawn::SetDriveInput(float Throttle, float Steering, bool bHandbrake)
{
	UChaosWheeledVehicleMovementComponent* Move = GetChaosMovement();
	if (!Move)
	{
		return;
	}

	if (!CanDrive())
	{
		Move->SetThrottleInput(0.f);
		Move->SetBrakeInput(0.f);
		Move->SetSteeringInput(0.f);
		Move->SetHandbrakeInput(true);
		bHandbrakeApplied = false;
		return;
	}
	bHandbrakeApplied = bHandbrake;

	if (IsStunned())
	{
		Throttle = 0.f;
	}

	Throttle = FMath::Clamp(Throttle, -1.f, 1.f);
	if (Throttle >= 0.f)
	{
		Move->SetThrottleInput(Throttle);
		Move->SetBrakeInput(0.f);
	}
	else
	{
		// With auto-reverse, braking at a standstill drives backwards.
		Move->SetThrottleInput(0.f);
		Move->SetBrakeInput(-Throttle);
	}
	Move->SetSteeringInput(FMath::Clamp(Steering, -1.f, 1.f));
	Move->SetHandbrakeInput(bHandbrake);
}

void AVMVehiclePawn::UpdateHandling()
{
	bHandlingDirty = false;
	UChaosWheeledVehicleMovementComponent* Move = GetChaosMovement();
	if (!Move || Move->WheelSetups.Num() < 4)
	{
		return;
	}

	const float Front = Damage->GetZoneFraction(EVMDamageZone::Front);
	const float Rear = Damage->GetZoneFraction(EVMDamageZone::Rear);
	const float Left = Damage->GetZoneFraction(EVMDamageZone::Left);
	const float Right = Damage->GetZoneFraction(EVMDamageZone::Right);

	// Front damage hurts the engine, rear damage hurts the drivetrain.
	Move->SetMaxEngineTorque(BaseMaxTorque * (0.35f + 0.65f * Front) * (0.75f + 0.25f * Rear));

	// Side damage costs grip on that side; oil costs grip everywhere.
	const float Slip = SlipTime > 0.f ? 0.2f : 1.f;
	Move->SetWheelFrictionMultiplier(0, BaseFrontFriction * (0.55f + 0.45f * Left) * Slip);
	Move->SetWheelFrictionMultiplier(1, BaseFrontFriction * (0.55f + 0.45f * Right) * Slip);
	Move->SetWheelFrictionMultiplier(2, BaseRearFriction * (0.55f + 0.45f * Left) * Slip);
	Move->SetWheelFrictionMultiplier(3, BaseRearFriction * (0.55f + 0.45f * Right) * Slip);

	// A battered front end or sides also cost steering lock.
	const float Steer = BaseSteerAngle * (0.45f + 0.55f * FMath::Min(Front, FMath::Min(Left, Right)));
	Move->SetWheelMaxSteerAngle(0, Steer);
	Move->SetWheelMaxSteerAngle(1, Steer);
}

EVMDamageZone AVMVehiclePawn::ZoneFromWorldPoint(const FVector& WorldPoint) const
{
	const FVector Local = GetActorTransform().InverseTransformPositionNoScale(WorldPoint);
	// Roughly 450cm long, 200cm wide.
	const float AX = FMath::Abs(Local.X) / 225.f;
	const float AY = FMath::Abs(Local.Y) / 100.f;
	if (AX >= AY)
	{
		return Local.X >= 0.f ? EVMDamageZone::Front : EVMDamageZone::Rear;
	}
	return Local.Y >= 0.f ? EVMDamageZone::Right : EVMDamageZone::Left;
}

void AVMVehiclePawn::OnChassisHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (IsWrecked() || !OtherActor || OtherActor == this)
	{
		return;
	}

	// Contact events repeat every physics step while touching; only count fresh impacts.
	const double Now = GetWorld()->GetTimeSeconds();
	if (const double* Last = LastHitTimes.Find(OtherActor))
	{
		if (Now - *Last < 0.35)
		{
			return;
		}
	}

	AVMVehiclePawn* OtherCar = Cast<AVMVehiclePawn>(OtherActor);
	const FVector MyVel = LastFrameVelocity;
	const FVector OtherVel = OtherCar ? OtherCar->GetLastFrameVelocity() : FVector::ZeroVector;
	const FVector Normal = Hit.ImpactNormal.GetSafeNormal();
	const float Closing = FMath::Abs(FVector::DotProduct(MyVel - OtherVel, Normal));
	if (Closing < MinImpactSpeed)
	{
		return;
	}
	LastHitTimes.Add(OtherActor, Now);

	float Amount;
	if (OtherCar)
	{
		// Whoever was driving into whom takes less of the damage.
		const FVector ToOther = (OtherCar->GetActorLocation() - GetActorLocation()).GetSafeNormal();
		const float MyPush = FMath::Max(0.f, FVector::DotProduct(MyVel, ToOther));
		const float TheirPush = FMath::Max(0.f, FVector::DotProduct(OtherVel, -ToOther));
		const float VictimShare = FMath::Clamp(TheirPush / (MyPush + TheirPush + 1.f), 0.15f, 1.f);
		Amount = (Closing - MinImpactSpeed) * RamDamageScale * VictimShare * OtherCar->GetRamDamageMultiplier();
		LastAttacker = OtherCar;
		LastAttackTime = Now;
	}
	else
	{
		Amount = (Closing - MinImpactSpeed) * WallDamageScale;
	}

	const float Applied = Damage->ApplyZoneDamage(ZoneFromWorldPoint(Hit.ImpactPoint), Amount, OtherActor);
	if (Applied > 0.f)
	{
		const bool bHeavy = Applied > 12.f;
		UVMSoundSubsystem::PlayAt(this, bHeavy ? EVMSound::ImpactHeavy : EVMSound::ImpactLight, Hit.ImpactPoint,
			FMath::Clamp(0.4f + Applied / 30.f, 0.4f, 1.f), FMath::FRandRange(0.85f, 1.15f));
		SpawnDebris(Hit.ImpactPoint, Applied > 15.f ? 3 : 1, 400.f);
		if (UVMEffectsSubsystem* FX = UVMEffectsSubsystem::Get(this))
		{
			FX->Sparks(Hit.ImpactPoint, Hit.ImpactNormal, FMath::Clamp(FMath::RoundToInt(Applied * 1.5f), 6, 60), bHeavy ? 1300.f : 800.f);
		}
		if (AVMGameMode* GM = GetVMGameMode())
		{
			GM->OnVehicleDamaged(this, OtherCar, Applied);
		}
	}
}

void AVMVehiclePawn::OnDamaged(EVMDamageZone Zone, float Amount, AActor* DamageCauser)
{
	bHandlingDirty = true;
}

void AVMVehiclePawn::OnPartBroken(EVMDamageZone Zone, int32 Stage)
{
	FVector Offset;
	switch (Zone)
	{
	case EVMDamageZone::Front: Offset = FVector(230.f, 0.f, 60.f); break;
	case EVMDamageZone::Rear:  Offset = FVector(-230.f, 0.f, 60.f); break;
	case EVMDamageZone::Left:  Offset = FVector(0.f, -110.f, 60.f); break;
	default:                   Offset = FVector(0.f, 110.f, 60.f); break;
	}
	SpawnDebris(GetActorTransform().TransformPosition(Offset), Stage == 2 ? 6 : 3, 700.f);
}

void AVMVehiclePawn::OnWrecked(AActor* FinalBlowCauser)
{
	bNitroHeld = false;
	UVMSoundSubsystem::PlayAt(this, EVMSound::Explosion, GetActorLocation(), 1.f, FMath::FRandRange(0.9f, 1.1f));
	if (UVMEffectsSubsystem* FX = UVMEffectsSubsystem::Get(this))
	{
		FX->Explosion(GetActorLocation() + FVector(0.f, 0.f, 80.f));
	}
	if (EngineAudio)
	{
		EngineAudio->FadeOut(0.5f, 0.f);
	}
	GetMesh()->AddImpulse(FVector(0.f, 0.f, 700.f), NAME_None, true);
	GetMesh()->AddAngularImpulseInDegrees(FVector(FMath::FRandRange(-200.f, 200.f), FMath::FRandRange(-200.f, 200.f), 0.f), NAME_None, true);
	SpawnDebris(GetActorLocation() + FVector(0.f, 0.f, 80.f), 10, 1200.f);

	AVMVehiclePawn* Credit = nullptr;
	if (LastAttacker.IsValid() && GetWorld()->GetTimeSeconds() - LastAttackTime < 6.0)
	{
		Credit = LastAttacker.Get();
	}
	if (AVMGameMode* GM = GetVMGameMode())
	{
		GM->OnVehicleWrecked(this, Credit);
	}
}

void AVMVehiclePawn::SpawnDebris(const FVector& WorldLocation, int32 Count, float Force)
{
	UWorld* World = GetWorld();
	if (!World || !DebrisMesh)
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	for (int32 i = 0; i < Count; ++i)
	{
		const FVector Loc = WorldLocation + FMath::VRand() * 30.f;
		AStaticMeshActor* Chunk = World->SpawnActor<AStaticMeshActor>(Loc, FRotator(FMath::FRandRange(0.f, 360.f), FMath::FRandRange(0.f, 360.f), 0.f), Params);
		if (!Chunk)
		{
			continue;
		}
		Chunk->SetMobility(EComponentMobility::Movable);
		UStaticMeshComponent* SM = Chunk->GetStaticMeshComponent();
		SM->SetStaticMesh(DebrisMesh);
		SM->SetWorldScale3D(FVector(FMath::FRandRange(0.1f, 0.35f), FMath::FRandRange(0.1f, 0.35f), FMath::FRandRange(0.03f, 0.1f)));
		SM->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
		SM->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		SM->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Ignore);
		SM->SetSimulatePhysics(true);
		VMUtil::Paint(SM, FLinearColor(0.08f, 0.08f, 0.09f));
		SM->AddImpulse((FMath::VRand() + FVector(0.f, 0.f, 1.f)) * Force + GetVelocity() * 0.5f, NAME_None, true);
		Chunk->SetLifeSpan(8.f);
	}
}

void AVMVehiclePawn::ReceivePickup(EVMPickupType Type)
{
	switch (Type)
	{
	case EVMPickupType::Nitro:     NitroCharge = FMath::Min(1.f, NitroCharge + 0.5f); break;
	case EVMPickupType::Repair:    Damage->RepairAll(); bHandlingDirty = true; break;
	case EVMPickupType::Armour:    ArmourTime = 15.f; break;
	case EVMPickupType::RamSpikes: SpikesTime = 15.f; break;
	case EVMPickupType::EMP:
	case EVMPickupType::OilSlick:  HeldPickup = Type; break;
	default: break;
	}
	UVMSoundSubsystem::PlayAt(this, EVMSound::Pickup, GetActorLocation(), bIsRival ? 0.5f : 0.9f);
	if (!bIsRival)
	{
		if (AVMGameMode* GM = GetVMGameMode())
		{
			GM->PushMessage(FString::Printf(TEXT("Picked up %s"), *VMPickups::GetName(Type)), VMPickups::GetColor(Type));
		}
	}
}

void AVMVehiclePawn::UseHeldPickup()
{
	if (!CanDrive() || HeldPickup == EVMPickupType::None)
	{
		return;
	}
	if (HeldPickup == EVMPickupType::EMP)
	{
		FireEMP();
	}
	else if (HeldPickup == EVMPickupType::OilSlick)
	{
		DropOilSlick();
	}
	HeldPickup = EVMPickupType::None;
}

void AVMVehiclePawn::FireEMP()
{
	AVMGameMode* GM = GetVMGameMode();
	UVMSoundSubsystem::PlayAt(this, EVMSound::EMP, GetActorLocation());
	if (UVMEffectsSubsystem* FX = UVMEffectsSubsystem::Get(this))
	{
		FX->EMPBurst(GetActorLocation(), 3000.f);
	}
	int32 Hits = 0;
	for (TActorIterator<AVMVehiclePawn> It(GetWorld()); It; ++It)
	{
		AVMVehiclePawn* Other = *It;
		if (Other == this || Other->IsWrecked() || FVector::Dist(Other->GetActorLocation(), GetActorLocation()) > 3000.f)
		{
			continue;
		}
		Other->ApplyStun(2.5f);
		Other->LastAttacker = this;
		Other->LastAttackTime = GetWorld()->GetTimeSeconds();
		const float Applied = Other->GetDamage()->ApplySpreadDamage(8.f, this);
		if (GM)
		{
			GM->OnVehicleDamaged(Other, this, Applied);
		}
		++Hits;
	}
	if (GM && !bIsRival)
	{
		GM->PushMessage(FString::Printf(TEXT("EMP blast hit %d car(s)"), Hits), VMPickups::GetColor(EVMPickupType::EMP));
	}
}

void AVMVehiclePawn::DropOilSlick()
{
	FVector Loc = GetActorLocation() - GetActorForwardVector() * 450.f;
	FVector Ground;
	if (VMUtil::GroundAt(GetWorld(), Loc, Ground))
	{
		Loc = Ground;
	}
	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	GetWorld()->SpawnActor<AVMOilSlick>(Loc, FRotator(0.f, GetActorRotation().Yaw, 0.f), Params);
	UVMSoundSubsystem::PlayAt(this, EVMSound::Oil, Loc);
	if (UVMEffectsSubsystem* FX = UVMEffectsSubsystem::Get(this))
	{
		FX->OilSplash(Loc);
	}
}

void AVMVehiclePawn::ApplyStun(float Seconds)
{
	StunTime = FMath::Max(StunTime, Seconds);
}

void AVMVehiclePawn::ApplySlip(float Seconds)
{
	SlipTime = FMath::Max(SlipTime, Seconds);
	bHandlingDirty = true;
}

void AVMVehiclePawn::SetBeaconColor(const FLinearColor& Color)
{
	BeaconColor = Color;
	if (HasActorBegunPlay())
	{
		VMUtil::Paint(Beacon, BeaconColor);
	}
}

void AVMVehiclePawn::Recover()
{
	if (IsWrecked() || RecoverCooldown > 0.f)
	{
		return;
	}
	RecoverCooldown = 2.f;

	const FRotator Upright(0.f, GetActorRotation().Yaw, 0.f);
	SetActorLocationAndRotation(GetActorLocation() + FVector(0.f, 0.f, 150.f), Upright, false, nullptr, ETeleportType::TeleportPhysics);
	GetMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector);
	GetMesh()->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	UVMSoundSubsystem::PlayAt(this, EVMSound::Recover, GetActorLocation(), 0.7f);

	if (!bIsRival)
	{
		if (AVMGameMode* GM = GetVMGameMode())
		{
			GM->AddTime(-3.f);
			GM->PushMessage(TEXT("Recovered (-3s)"), FLinearColor(1.f, 0.7f, 0.2f));
		}
	}
}

void AVMVehiclePawn::ToggleCamera()
{
	bHoodCamera = !bHoodCamera;
	if (bHoodCamera)
	{
		SpringArm->TargetArmLength = 0.f;
		SpringArm->SetRelativeLocation(FVector(60.f, 0.f, 115.f));
		SpringArm->SetRelativeRotation(FRotator::ZeroRotator);
		SpringArm->bEnableCameraLag = false;
		SpringArm->bEnableCameraRotationLag = false;
	}
	else
	{
		SpringArm->TargetArmLength = 650.f;
		SpringArm->SetRelativeLocation(FVector(0.f, 0.f, 140.f));
		SpringArm->SetRelativeRotation(FRotator(-12.f, 0.f, 0.f));
		SpringArm->bEnableCameraLag = true;
		SpringArm->bEnableCameraRotationLag = true;
	}
}

// ---------------------------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------------------------

namespace
{
	UInputAction* MakeAction(UObject* Outer, const TCHAR* Name, EInputActionValueType Type)
	{
		UInputAction* Action = NewObject<UInputAction>(Outer, Name);
		Action->ValueType = Type;
		return Action;
	}

	void MapNegated(UInputMappingContext* Context, UInputAction* Action, const FKey& Key)
	{
		FEnhancedActionKeyMapping& Mapping = Context->MapKey(Action, Key);
		Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(Context));
	}
}

void AVMVehiclePawn::BuildInputAssets()
{
	if (InputContext)
	{
		return;
	}

	ThrottleAction = MakeAction(this, TEXT("IA_Throttle"), EInputActionValueType::Axis1D);
	SteerAction = MakeAction(this, TEXT("IA_Steer"), EInputActionValueType::Axis1D);
	HandbrakeAction = MakeAction(this, TEXT("IA_Handbrake"), EInputActionValueType::Boolean);
	NitroAction = MakeAction(this, TEXT("IA_Nitro"), EInputActionValueType::Boolean);
	UsePickupAction = MakeAction(this, TEXT("IA_UsePickup"), EInputActionValueType::Boolean);
	RecoverAction = MakeAction(this, TEXT("IA_Recover"), EInputActionValueType::Boolean);
	CameraAction = MakeAction(this, TEXT("IA_Camera"), EInputActionValueType::Boolean);
	RestartAction = MakeAction(this, TEXT("IA_Restart"), EInputActionValueType::Boolean);
	PauseAction = MakeAction(this, TEXT("IA_Pause"), EInputActionValueType::Boolean);
	PauseAction->bTriggerWhenPaused = true;
	NavUpAction = MakeAction(this, TEXT("IA_NavUp"), EInputActionValueType::Boolean);
	NavDownAction = MakeAction(this, TEXT("IA_NavDown"), EInputActionValueType::Boolean);
	NavLeftAction = MakeAction(this, TEXT("IA_NavLeft"), EInputActionValueType::Boolean);
	NavRightAction = MakeAction(this, TEXT("IA_NavRight"), EInputActionValueType::Boolean);
	BackAction = MakeAction(this, TEXT("IA_Back"), EInputActionValueType::Boolean);
	// Menu actions must keep working while the game is paused (pause menu).
	for (UInputAction* MenuAction : { NavUpAction.Get(), NavDownAction.Get(), NavLeftAction.Get(), NavRightAction.Get(), RestartAction.Get(), BackAction.Get() })
	{
		MenuAction->bTriggerWhenPaused = true;
	}

	InputContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Vehicle"));

	InputContext->MapKey(ThrottleAction, EKeys::W);
	InputContext->MapKey(ThrottleAction, EKeys::Up);
	InputContext->MapKey(ThrottleAction, EKeys::Gamepad_RightTriggerAxis);
	MapNegated(InputContext, ThrottleAction, EKeys::S);
	MapNegated(InputContext, ThrottleAction, EKeys::Down);
	MapNegated(InputContext, ThrottleAction, EKeys::Gamepad_LeftTriggerAxis);

	InputContext->MapKey(SteerAction, EKeys::D);
	InputContext->MapKey(SteerAction, EKeys::Right);
	InputContext->MapKey(SteerAction, EKeys::Gamepad_LeftX);
	MapNegated(InputContext, SteerAction, EKeys::A);
	MapNegated(InputContext, SteerAction, EKeys::Left);

	InputContext->MapKey(HandbrakeAction, EKeys::SpaceBar);
	InputContext->MapKey(HandbrakeAction, EKeys::Gamepad_FaceButton_Right);
	InputContext->MapKey(NitroAction, EKeys::LeftShift);
	InputContext->MapKey(NitroAction, EKeys::Gamepad_FaceButton_Bottom);
	InputContext->MapKey(UsePickupAction, EKeys::E);
	InputContext->MapKey(UsePickupAction, EKeys::Gamepad_FaceButton_Left);
	InputContext->MapKey(RecoverAction, EKeys::R);
	InputContext->MapKey(RecoverAction, EKeys::Gamepad_FaceButton_Top);
	InputContext->MapKey(CameraAction, EKeys::C);
	InputContext->MapKey(CameraAction, EKeys::Gamepad_RightThumbstick);
	InputContext->MapKey(RestartAction, EKeys::Enter);
	InputContext->MapKey(RestartAction, EKeys::Gamepad_Special_Left);
	InputContext->MapKey(RestartAction, EKeys::Gamepad_FaceButton_Bottom);
	InputContext->MapKey(NavUpAction, EKeys::Up);
	InputContext->MapKey(NavUpAction, EKeys::W);
	InputContext->MapKey(NavUpAction, EKeys::Gamepad_DPad_Up);
	InputContext->MapKey(NavDownAction, EKeys::Down);
	InputContext->MapKey(NavDownAction, EKeys::S);
	InputContext->MapKey(NavDownAction, EKeys::Gamepad_DPad_Down);
	InputContext->MapKey(NavLeftAction, EKeys::Left);
	InputContext->MapKey(NavLeftAction, EKeys::A);
	InputContext->MapKey(NavLeftAction, EKeys::Gamepad_DPad_Left);
	InputContext->MapKey(NavRightAction, EKeys::Right);
	InputContext->MapKey(NavRightAction, EKeys::D);
	InputContext->MapKey(NavRightAction, EKeys::Gamepad_DPad_Right);
	InputContext->MapKey(BackAction, EKeys::Escape);
	InputContext->MapKey(BackAction, EKeys::BackSpace);
	InputContext->MapKey(BackAction, EKeys::Gamepad_FaceButton_Right);
	InputContext->MapKey(PauseAction, EKeys::P);
	InputContext->MapKey(PauseAction, EKeys::Gamepad_Special_Right);
}

void AVMVehiclePawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	BuildInputAssets();

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (ULocalPlayer* LP = PC->GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			{
				Subsystem->ClearAllMappings();
				Subsystem->AddMappingContext(InputContext, 0);
			}
		}
	}

	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!Input)
	{
		UE_LOG(LogVehicular, Error, TEXT("Enhanced Input is not the default input component. Check Config/DefaultInput.ini."));
		return;
	}

	Input->BindAction(ThrottleAction, ETriggerEvent::Triggered, this, &AVMVehiclePawn::OnThrottleInput);
	Input->BindAction(ThrottleAction, ETriggerEvent::Completed, this, &AVMVehiclePawn::OnThrottleInput);
	Input->BindAction(SteerAction, ETriggerEvent::Triggered, this, &AVMVehiclePawn::OnSteerInput);
	Input->BindAction(SteerAction, ETriggerEvent::Completed, this, &AVMVehiclePawn::OnSteerInput);
	Input->BindAction(HandbrakeAction, ETriggerEvent::Triggered, this, &AVMVehiclePawn::OnHandbrakeInput);
	Input->BindAction(HandbrakeAction, ETriggerEvent::Completed, this, &AVMVehiclePawn::OnHandbrakeInput);
	Input->BindAction(NitroAction, ETriggerEvent::Triggered, this, &AVMVehiclePawn::OnNitroInput);
	Input->BindAction(NitroAction, ETriggerEvent::Completed, this, &AVMVehiclePawn::OnNitroInput);
	Input->BindAction(UsePickupAction, ETriggerEvent::Started, this, &AVMVehiclePawn::OnUsePickupInput);
	Input->BindAction(RecoverAction, ETriggerEvent::Started, this, &AVMVehiclePawn::OnRecoverInput);
	Input->BindAction(CameraAction, ETriggerEvent::Started, this, &AVMVehiclePawn::OnCameraInput);
	Input->BindAction(RestartAction, ETriggerEvent::Started, this, &AVMVehiclePawn::OnRestartInput);
	Input->BindAction(PauseAction, ETriggerEvent::Started, this, &AVMVehiclePawn::OnPauseInput);
	Input->BindAction(NavUpAction, ETriggerEvent::Started, this, &AVMVehiclePawn::OnNavUpInput);
	Input->BindAction(NavDownAction, ETriggerEvent::Started, this, &AVMVehiclePawn::OnNavDownInput);
	Input->BindAction(NavLeftAction, ETriggerEvent::Started, this, &AVMVehiclePawn::OnNavLeftInput);
	Input->BindAction(NavRightAction, ETriggerEvent::Started, this, &AVMVehiclePawn::OnNavRightInput);
	Input->BindAction(BackAction, ETriggerEvent::Started, this, &AVMVehiclePawn::OnBackInput);
}

void AVMVehiclePawn::OnThrottleInput(const FInputActionValue& Value) { PlayerThrottle = Value.Get<float>(); }
void AVMVehiclePawn::OnSteerInput(const FInputActionValue& Value) { PlayerSteer = Value.Get<float>(); }
void AVMVehiclePawn::OnHandbrakeInput(const FInputActionValue& Value) { bPlayerHandbrake = Value.Get<bool>(); }
void AVMVehiclePawn::OnNitroInput(const FInputActionValue& Value) { bNitroHeld = Value.Get<bool>(); }
void AVMVehiclePawn::OnUsePickupInput() { UseHeldPickup(); }
void AVMVehiclePawn::OnRecoverInput() { Recover(); }
void AVMVehiclePawn::OnCameraInput() { ToggleCamera(); }

void AVMVehiclePawn::OnRestartInput()
{
	if (AVMGameMode* GM = GetVMGameMode())
	{
		GM->HandleConfirm();
	}
}

void AVMVehiclePawn::OnNavUpInput()
{
	if (AVMGameMode* GM = GetVMGameMode())
	{
		GM->NavigateGarage(-1);
	}
}

void AVMVehiclePawn::OnNavDownInput()
{
	if (AVMGameMode* GM = GetVMGameMode())
	{
		GM->NavigateGarage(1);
	}
}

void AVMVehiclePawn::OnPauseInput()
{
	if (AVMGameMode* GM = GetVMGameMode())
	{
		GM->TogglePause();
	}
}

void AVMVehiclePawn::OnBackInput()
{
	if (AVMGameMode* GM = GetVMGameMode())
	{
		GM->HandleBack();
	}
}

void AVMVehiclePawn::OnNavLeftInput()
{
	if (AVMGameMode* GM = GetVMGameMode())
	{
		GM->NavigateGarageSide(-1);
	}
}

void AVMVehiclePawn::OnNavRightInput()
{
	if (AVMGameMode* GM = GetVMGameMode())
	{
		GM->NavigateGarageSide(1);
	}
}

// ---------------------------------------------------------------------------------------------
// Car types
// ---------------------------------------------------------------------------------------------

void AVMVehiclePawn::ApplyCarDefinition()
{
	const FVMCarDefinition* Found = UVMCarCatalog::Find(CarId);
	const FVMCarDefinition& Car = Found ? *Found : UVMCarCatalog::GetStarter();
	CarId = Car.Id;

	if (UChaosWheeledVehicleMovementComponent* Move = GetChaosMovement())
	{
		if (!FMath::IsNearlyEqual(Move->Mass, Car.MassKg))
		{
			Move->Mass = Car.MassKg;
			Move->RecreatePhysicsState(); // the vehicle sim reads mass when its physics state is built
		}
	}
	BaseMaxTorque *= Car.TorqueMultiplier;
	BaseFrontFriction *= Car.GripMultiplier;
	BaseRearFriction *= Car.GripMultiplier;
	BaseSteerAngle *= Car.SteerMultiplier;
	Damage->SetMaxZoneHealth(100.f * Car.HealthMultiplier);
	RamMultiplier = Car.RamMultiplier;
	NitroAcceleration *= Car.NitroMultiplier;
	bHandlingDirty = true;

	// Bolt-on parts so each car type is recognisable. Offsets assume a car about 4.5m x 2m.
	const FLinearColor Kit = Car.KitColor;
	const FLinearColor Steel(0.6f, 0.6f, 0.62f);
	for (EVMBodyKit Part : Car.BodyKits)
	{
		switch (Part)
		{
		case EVMBodyKit::BullBar:
			AddKitPart(TEXT("Cube"), FVector(238.f, 0.f, 55.f), FRotator::ZeroRotator, FVector(0.12f, 1.8f, 0.12f), Kit);
			AddKitPart(TEXT("Cube"), FVector(238.f, 0.f, 85.f), FRotator::ZeroRotator, FVector(0.12f, 1.6f, 0.1f), Kit);
			AddKitPart(TEXT("Cube"), FVector(236.f, -55.f, 65.f), FRotator::ZeroRotator, FVector(0.1f, 0.1f, 0.5f), Kit);
			AddKitPart(TEXT("Cube"), FVector(236.f, 55.f, 65.f), FRotator::ZeroRotator, FVector(0.1f, 0.1f, 0.5f), Kit);
			break;
		case EVMBodyKit::RoofRack:
			AddKitPart(TEXT("Cube"), FVector(-20.f, -55.f, 150.f), FRotator::ZeroRotator, FVector(1.6f, 0.06f, 0.06f), Kit);
			AddKitPart(TEXT("Cube"), FVector(-20.f, 55.f, 150.f), FRotator::ZeroRotator, FVector(1.6f, 0.06f, 0.06f), Kit);
			AddKitPart(TEXT("Cube"), FVector(30.f, 0.f, 152.f), FRotator::ZeroRotator, FVector(0.06f, 1.2f, 0.06f), Kit);
			AddKitPart(TEXT("Cube"), FVector(-70.f, 0.f, 152.f), FRotator::ZeroRotator, FVector(0.06f, 1.2f, 0.06f), Kit);
			AddKitPart(TEXT("Sphere"), FVector(45.f, -35.f, 165.f), FRotator::ZeroRotator, FVector(0.22f), FLinearColor(1.f, 0.95f, 0.6f));
			AddKitPart(TEXT("Sphere"), FVector(45.f, 35.f, 165.f), FRotator::ZeroRotator, FVector(0.22f), FLinearColor(1.f, 0.95f, 0.6f));
			break;
		case EVMBodyKit::Spoiler:
			AddKitPart(TEXT("Cube"), FVector(-215.f, 0.f, 128.f), FRotator(-6.f, 0.f, 0.f), FVector(0.35f, 1.8f, 0.04f), Kit);
			AddKitPart(TEXT("Cube"), FVector(-210.f, -60.f, 108.f), FRotator::ZeroRotator, FVector(0.12f, 0.04f, 0.4f), Kit);
			AddKitPart(TEXT("Cube"), FVector(-210.f, 60.f, 108.f), FRotator::ZeroRotator, FVector(0.12f, 0.04f, 0.4f), Kit);
			break;
		case EVMBodyKit::SpikeRam:
			for (int32 i = -2; i <= 2; ++i)
			{
				AddKitPart(TEXT("Cone"), FVector(250.f, i * 38.f, 50.f), FRotator(-90.f, 0.f, 0.f), FVector(0.22f, 0.22f, 0.55f), Steel);
			}
			break;
		case EVMBodyKit::ArmourPlates:
			AddKitPart(TEXT("Cube"), FVector(0.f, -106.f, 60.f), FRotator::ZeroRotator, FVector(3.4f, 0.06f, 0.5f), Kit);
			AddKitPart(TEXT("Cube"), FVector(0.f, 106.f, 60.f), FRotator::ZeroRotator, FVector(3.4f, 0.06f, 0.5f), Kit);
			AddKitPart(TEXT("Cube"), FVector(225.f, 0.f, 70.f), FRotator(-15.f, 0.f, 0.f), FVector(0.08f, 1.9f, 0.55f), Kit);
			break;
		case EVMBodyKit::SideExhausts:
			AddKitPart(TEXT("Cylinder"), FVector(-60.f, -103.f, 35.f), FRotator(90.f, 0.f, 0.f), FVector(0.12f, 0.12f, 1.4f), Steel);
			AddKitPart(TEXT("Cylinder"), FVector(-60.f, 103.f, 35.f), FRotator(90.f, 0.f, 0.f), FVector(0.12f, 0.12f, 1.4f), Steel);
			break;
		}
	}
}

void AVMVehiclePawn::AddKitPart(const TCHAR* Shape, const FVector& Location, const FRotator& Rotation, const FVector& Scale, const FLinearColor& Color)
{
	UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(this);
	Part->SetStaticMesh(VMUtil::Shape(Shape));
	Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Part->SetupAttachment(GetMesh());
	Part->SetRelativeTransform(FTransform(Rotation, Location, Scale));
	Part->RegisterComponent();
	AddInstanceComponent(Part);
	VMUtil::Paint(Part, Color);
}

// ---------------------------------------------------------------------------------------------
// Particles
// ---------------------------------------------------------------------------------------------

void AVMVehiclePawn::UpdateEffects(float DeltaSeconds)
{
	UVMEffectsSubsystem* FX = UVMEffectsSubsystem::Get(this);
	if (!FX)
	{
		return;
	}
	const FTransform& T = GetActorTransform();
	const FVector Vel = GetVelocity();

	// Engine smoke once the car is hurt; black smoke and fire once it's wrecked.
	const float Health = Damage->GetTotalFraction();
	float SmokeRate = 0.f;
	bool bDarkSmoke = false;
	if (IsWrecked())
	{
		SmokeRate = 25.f;
		bDarkSmoke = true;
		FireAccum += 14.f * DeltaSeconds;
		while (FireAccum >= 1.f)
		{
			FireAccum -= 1.f;
			FX->FirePuff(T.TransformPosition(FVector(FMath::FRandRange(-150.f, 200.f), FMath::FRandRange(-60.f, 60.f), 90.f)));
		}
	}
	else if (Health < 0.6f)
	{
		SmokeRate = FMath::Lerp(3.f, 20.f, FMath::Clamp((0.6f - Health) / 0.35f, 0.f, 1.f));
		bDarkSmoke = Health < 0.4f;
	}
	SmokeAccum += SmokeRate * DeltaSeconds;
	while (SmokeAccum >= 1.f)
	{
		SmokeAccum -= 1.f;
		FX->SmokePuff(T.TransformPosition(FVector(170.f, FMath::FRandRange(-40.f, 40.f), 95.f)), Vel * 0.3f + FVector(0.f, 0.f, 120.f), bDarkSmoke, IsWrecked() ? 110.f : 70.f);
	}

	// Tyre smoke when sliding or handbraking at speed.
	const float Speed = Vel.Size();
	const float Lateral = FMath::Abs(FVector::DotProduct(Vel, GetActorRightVector()));
	const bool bSliding = !IsWrecked() && Speed > 700.f && (Lateral > 450.f || (bHandbrakeApplied && Speed > 900.f));
	if (bSliding)
	{
		TyreAccum += 40.f * DeltaSeconds;
		while (TyreAccum >= 1.f)
		{
			TyreAccum -= 1.f;
			TyreSide ^= 1;
			FX->TyreSmoke(T.TransformPosition(FVector(-140.f, TyreSide ? 85.f : -85.f, 15.f)), Vel);
		}
	}

	// Exhaust flames while boosting.
	if (bWasBoosting)
	{
		FlameAccum += 70.f * DeltaSeconds;
		while (FlameAccum >= 1.f)
		{
			FlameAccum -= 1.f;
			TyreSide ^= 1;
			FX->NitroFlame(T.TransformPosition(FVector(-235.f, TyreSide ? 35.f : -35.f, 45.f)), -GetActorForwardVector(), Vel);
		}
	}
}
