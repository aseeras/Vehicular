#pragma once

#include "CoreMinimal.h"
#include "WheeledVehiclePawn.h"
#include "VMTypes.h"
#include "VMVehiclePawn.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UStaticMeshComponent;
class UStaticMesh;
class UVMDamageComponent;
class UInputAction;
class UInputMappingContext;
class UChaosWheeledVehicleMovementComponent;
class UAudioComponent;
class AVMGameMode;
struct FInputActionValue;

/**
 * The car used by both the player and rivals. Player input and the rival AI both drive it
 * through SetDriveInput(), so every car follows the same handling, damage and pickup rules.
 */
UCLASS()
class VEHICULAR_API AVMVehiclePawn : public AWheeledVehiclePawn
{
	GENERATED_BODY()

public:
	AVMVehiclePawn(const FObjectInitializer& ObjectInitializer);

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	// --- Driving API (player + AI) ---
	UFUNCTION(BlueprintCallable, Category = "Vehicular")
	void SetDriveInput(float Throttle, float Steering, bool bHandbrake);

	UFUNCTION(BlueprintCallable, Category = "Vehicular")
	void SetNitroHeld(bool bHeld) { bNitroHeld = bHeld; }

	UFUNCTION(BlueprintCallable, Category = "Vehicular")
	void UseHeldPickup();

	/** Puts the car back on its wheels. The player pays a small time penalty. */
	UFUNCTION(BlueprintCallable, Category = "Vehicular")
	void Recover();

	UFUNCTION(BlueprintCallable, Category = "Vehicular")
	void ReceivePickup(EVMPickupType Type);

	void ApplyStun(float Seconds);
	void ApplySlip(float Seconds);
	void SetBeaconColor(const FLinearColor& Color);

	// --- Queries ---
	UFUNCTION(BlueprintPure, Category = "Vehicular")
	float GetSpeedKmh() const;

	UFUNCTION(BlueprintPure, Category = "Vehicular")
	bool IsWrecked() const;

	UFUNCTION(BlueprintPure, Category = "Vehicular")
	bool IsUpsideDown() const;

	UVMDamageComponent* GetDamage() const { return Damage; }
	float GetNitroCharge() const { return NitroCharge; }
	EVMPickupType GetHeldPickup() const { return HeldPickup; }
	float GetArmourTimeLeft() const { return ArmourTime; }
	float GetSpikesTimeLeft() const { return SpikesTime; }
	bool IsStunned() const { return StunTime > 0.f; }
	float GetRamDamageMultiplier() const { return RamMultiplier * (SpikesTime > 0.f ? 2.f : 1.f); }
	FVector GetLastFrameVelocity() const { return LastFrameVelocity; }
	bool HasVehicleMesh() const;
	UChaosWheeledVehicleMovementComponent* GetChaosMovement() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicular")
	FString DriverName = TEXT("You");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicular")
	bool bIsRival = false;

	/** Garage upgrades. The player's are loaded from the career save; rivals get theirs from the game mode. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicular")
	FVMUpgradeLevels Upgrades;

	/** Which car from Project Settings > Vehicular Cars this is. The player's comes from the career save. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicular")
	FName CarId;

	/** Extra forward acceleration while boosting (cm/s^2). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicular")
	float NitroAcceleration = 1500.f;

	/** Seconds a full nitro tank lasts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicular")
	float NitroDuration = 3.f;

	/** Impacts slower than this (cm/s) do no damage. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicular|Damage")
	float MinImpactSpeed = 400.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicular|Damage")
	float WallDamageScale = 0.012f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicular|Damage")
	float RamDamageScale = 0.025f;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UVMDamageComponent> Damage;

	/** Coloured marker above rival cars so you can tell them apart. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Beacon;

	UFUNCTION()
	void OnChassisHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	UFUNCTION()
	void OnDamaged(EVMDamageZone Zone, float Amount, AActor* DamageCauser);

	UFUNCTION()
	void OnPartBroken(EVMDamageZone Zone, int32 Stage);

	UFUNCTION()
	void OnWrecked(AActor* FinalBlowCauser);

private:
	AVMGameMode* GetVMGameMode() const;
	bool CanDrive() const;
	void UpdateHandling();
	void SpawnDebris(const FVector& WorldLocation, int32 Count, float Force);
	void FireEMP();
	void DropOilSlick();
	void ToggleCamera();
	void ApplyUpgrades();
	void ApplyCarDefinition();
	void AddKitPart(const TCHAR* Shape, const FVector& Location, const FRotator& Rotation, const FVector& Scale, const FLinearColor& Color);
	void UpdateEffects(float DeltaSeconds);
	void UpdateEngineSound(float DeltaSeconds);
	EVMDamageZone ZoneFromWorldPoint(const FVector& WorldPoint) const;

	// Input (assets are built in code so the project needs no input assets)
	void BuildInputAssets();
	void OnThrottleInput(const FInputActionValue& Value);
	void OnSteerInput(const FInputActionValue& Value);
	void OnHandbrakeInput(const FInputActionValue& Value);
	void OnNitroInput(const FInputActionValue& Value);
	void OnUsePickupInput();
	void OnRecoverInput();
	void OnCameraInput();
	void OnRestartInput();
	void OnNavUpInput();
	void OnNavDownInput();
	void OnNavLeftInput();
	void OnNavRightInput();
	void OnBackInput();
	void OnPauseInput();

	UPROPERTY(Transient) TObjectPtr<UInputMappingContext> InputContext;
	UPROPERTY(Transient) TObjectPtr<UInputAction> ThrottleAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> SteerAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> HandbrakeAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> NitroAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> UsePickupAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> RecoverAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> CameraAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> RestartAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> PauseAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> NavUpAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> NavDownAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> NavLeftAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> NavRightAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> BackAction;

	UPROPERTY(Transient) TObjectPtr<UAudioComponent> EngineAudio;

	UPROPERTY() TObjectPtr<UStaticMesh> DebrisMesh;

	float PlayerThrottle = 0.f;
	float PlayerSteer = 0.f;
	bool bPlayerHandbrake = false;

	bool bNitroHeld = false;
	float NitroCharge = 0.5f;
	EVMPickupType HeldPickup = EVMPickupType::None;
	float ArmourTime = 0.f;
	float SpikesTime = 0.f;
	float StunTime = 0.f;
	float SlipTime = 0.f;
	float RecoverCooldown = 0.f;
	bool bHoodCamera = false;
	bool bHandlingDirty = true;
	bool bWasBoosting = false;
	bool bHandbrakeApplied = false;
	float SmokeAccum = 0.f;
	float FireAccum = 0.f;
	float TyreAccum = 0.f;
	float FlameAccum = 0.f;
	int32 TyreSide = 0;
	float MenuOrbitYaw = 0.f;
	float EngineVolume = 0.f;

	// Upgrade multipliers.
	float ArmourMultiplier = 1.f;
	float RamMultiplier = 1.f;

	float BaseMaxTorque = 750.f;
	float BaseFrontFriction = 3.f;
	float BaseRearFriction = 3.f;
	float BaseSteerAngle = 40.f;

	FVector LastFrameVelocity = FVector::ZeroVector;
	FLinearColor BeaconColor = FLinearColor::Red;

	TMap<TWeakObjectPtr<AActor>, double> LastHitTimes;
	TWeakObjectPtr<AVMVehiclePawn> LastAttacker;
	double LastAttackTime = -100.0;
};
