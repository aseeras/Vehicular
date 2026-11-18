#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "VMTypes.h"
#include "VMSoundSubsystem.generated.h"

class UAudioComponent;
class USoundAttenuation;
class USoundBase;
class USoundWaveProcedural;

/** Plays the game's sound effects. Usage: UVMSoundSubsystem::PlayAt(this, EVMSound::Squash, Location). */
UCLASS()
class VEHICULAR_API UVMSoundSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	static UVMSoundSubsystem* Get(const UObject* WorldContext);

	/** Convenience wrappers that do nothing if there is no world or subsystem. */
	static void PlayAt(const UObject* WorldContext, EVMSound Sound, const FVector& Location, float Volume = 1.f, float Pitch = 1.f);
	static void Play2D(const UObject* WorldContext, EVMSound Sound, float Volume = 1.f, float Pitch = 1.f);

	void PlaySoundAt(EVMSound Sound, const FVector& Location, float Volume, float Pitch);
	void PlaySound2D(EVMSound Sound, float Volume, float Pitch);

	/** Creates a looping engine sound attached to a component. Pitch it with SetPitchMultiplier. */
	UAudioComponent* CreateEngineLoop(USceneComponent* AttachTo);

private:
	USoundBase* ResolveSound(EVMSound Sound, float& OutDuration);
	bool PassesRateLimit(EVMSound Sound);
	void TrackOneShot(UAudioComponent* Component, float Duration);

	UPROPERTY() TObjectPtr<USoundAttenuation> Attenuation;

	struct FActiveOneShot
	{
		TWeakObjectPtr<UAudioComponent> Component;
		double EndTime = 0.0;
	};
	TArray<FActiveOneShot> ActiveOneShots;

	TMap<EVMSound, TSharedPtr<TArray<int16>, ESPMode::ThreadSafe>> PCMCache;
	TSharedPtr<TArray<int16>, ESPMode::ThreadSafe> EnginePCM;
	TMap<EVMSound, double> LastPlayed;
};
