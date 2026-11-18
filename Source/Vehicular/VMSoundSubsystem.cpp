#include "VMSoundSubsystem.h"
#include "VMAudioSettings.h"
#include "VMSynth.h"

#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundWaveProcedural.h"

namespace
{
	float MinInterval(EVMSound Sound)
	{
		switch (Sound)
		{
		case EVMSound::Yelp:        return 0.25f;
		case EVMSound::ImpactLight: return 0.05f;
		case EVMSound::Squash:      return 0.05f;
		default:                    return 0.f;
		}
	}

	USoundWaveProcedural* MakeWave(UObject* Outer, float Duration)
	{
		USoundWaveProcedural* Wave = NewObject<USoundWaveProcedural>(Outer);
		Wave->SetSampleRate(VMSynth::SampleRate);
		Wave->NumChannels = 1;
		Wave->Duration = Duration;
		Wave->SoundGroup = SOUNDGROUP_Default;
		Wave->bLooping = false;
		return Wave;
	}
}

void UVMSoundSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Attenuation = NewObject<USoundAttenuation>(this);
	FSoundAttenuationSettings& Settings = Attenuation->Attenuation;
	Settings.bAttenuate = true;
	Settings.bSpatialize = true;
	Settings.AttenuationShape = EAttenuationShape::Sphere;
	Settings.AttenuationShapeExtents = FVector(600.f, 0.f, 0.f);
	Settings.FalloffDistance = 7000.f;
}

TStatId UVMSoundSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UVMSoundSubsystem, STATGROUP_Tickables);
}

UVMSoundSubsystem* UVMSoundSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UVMSoundSubsystem>() : nullptr;
}

void UVMSoundSubsystem::PlayAt(const UObject* WorldContext, EVMSound Sound, const FVector& Location, float Volume, float Pitch)
{
	if (UVMSoundSubsystem* Subsystem = Get(WorldContext))
	{
		Subsystem->PlaySoundAt(Sound, Location, Volume, Pitch);
	}
}

void UVMSoundSubsystem::Play2D(const UObject* WorldContext, EVMSound Sound, float Volume, float Pitch)
{
	if (UVMSoundSubsystem* Subsystem = Get(WorldContext))
	{
		Subsystem->PlaySound2D(Sound, Volume, Pitch);
	}
}

bool UVMSoundSubsystem::PassesRateLimit(EVMSound Sound)
{
	const double Now = GetWorld()->GetRealTimeSeconds();
	double& Last = LastPlayed.FindOrAdd(Sound, -100.0);
	if (Now - Last < MinInterval(Sound))
	{
		return false;
	}
	Last = Now;
	return true;
}

USoundBase* UVMSoundSubsystem::ResolveSound(EVMSound Sound, float& OutDuration)
{
	OutDuration = -1.f; // -1: an asset that manages its own length

	const UVMAudioSettings* Settings = GetDefault<UVMAudioSettings>();
	if (const TSoftObjectPtr<USoundBase>* Override = Settings->SoundOverrides.Find(Sound))
	{
		if (!Override->IsNull())
		{
			if (USoundBase* Loaded = Override->LoadSynchronous())
			{
				return Loaded;
			}
		}
	}

	TSharedPtr<TArray<int16>, ESPMode::ThreadSafe>& PCM = PCMCache.FindOrAdd(Sound);
	if (!PCM.IsValid())
	{
		PCM = MakeShared<TArray<int16>, ESPMode::ThreadSafe>(VMSynth::Generate(Sound));
	}
	if (PCM->Num() == 0)
	{
		return nullptr;
	}

	// A procedural wave consumes its queue as it plays, so each one-shot gets its own.
	OutDuration = static_cast<float>(PCM->Num()) / VMSynth::SampleRate;
	USoundWaveProcedural* Wave = MakeWave(this, OutDuration);
	Wave->QueueAudio(reinterpret_cast<const uint8*>(PCM->GetData()), PCM->Num() * sizeof(int16));
	return Wave;
}

void UVMSoundSubsystem::TrackOneShot(UAudioComponent* Component, float Duration)
{
	if (Component && Duration > 0.f)
	{
		ActiveOneShots.Add({ Component, GetWorld()->GetRealTimeSeconds() + Duration + 0.05 });
	}
}

void UVMSoundSubsystem::PlaySoundAt(EVMSound Sound, const FVector& Location, float Volume, float Pitch)
{
	if (!PassesRateLimit(Sound))
	{
		return;
	}
	float Duration;
	if (USoundBase* Asset = ResolveSound(Sound, Duration))
	{
		const float FinalVolume = Volume * GetDefault<UVMAudioSettings>()->EffectsVolume;
		UAudioComponent* AC = UGameplayStatics::SpawnSoundAtLocation(this, Asset, Location, FRotator::ZeroRotator, FinalVolume, Pitch, 0.f, Attenuation);
		TrackOneShot(AC, Duration / FMath::Max(0.1f, Pitch));
	}
}

void UVMSoundSubsystem::PlaySound2D(EVMSound Sound, float Volume, float Pitch)
{
	if (!PassesRateLimit(Sound))
	{
		return;
	}
	float Duration;
	if (USoundBase* Asset = ResolveSound(Sound, Duration))
	{
		const float FinalVolume = Volume * GetDefault<UVMAudioSettings>()->EffectsVolume;
		UAudioComponent* AC = UGameplayStatics::SpawnSound2D(this, Asset, FinalVolume, Pitch);
		TrackOneShot(AC, Duration / FMath::Max(0.1f, Pitch));
	}
}

void UVMSoundSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	// Procedural waves don't end by themselves; stop each one when its samples run out.
	const double Now = GetWorld()->GetRealTimeSeconds();
	for (int32 i = ActiveOneShots.Num() - 1; i >= 0; --i)
	{
		UAudioComponent* AC = ActiveOneShots[i].Component.Get();
		if (!AC)
		{
			ActiveOneShots.RemoveAtSwap(i);
		}
		else if (Now >= ActiveOneShots[i].EndTime)
		{
			AC->Stop();
			ActiveOneShots.RemoveAtSwap(i);
		}
	}
}

UAudioComponent* UVMSoundSubsystem::CreateEngineLoop(USceneComponent* AttachTo)
{
	if (!AttachTo)
	{
		return nullptr;
	}

	const UVMAudioSettings* Settings = GetDefault<UVMAudioSettings>();
	USoundBase* Sound = Settings->EngineLoopOverride.IsNull() ? nullptr : Settings->EngineLoopOverride.LoadSynchronous();
	if (!Sound)
	{
		if (!EnginePCM.IsValid())
		{
			EnginePCM = MakeShared<TArray<int16>, ESPMode::ThreadSafe>(VMSynth::EngineLoop());
		}
		USoundWaveProcedural* Wave = MakeWave(AttachTo, INDEFINITELY_LOOPING_DURATION);
		TSharedPtr<TArray<int16>, ESPMode::ThreadSafe> Buffer = EnginePCM;
		// Runs on the audio thread: keep re-queueing the same immutable buffer.
		Wave->OnSoundWaveProceduralUnderflow.BindLambda([Buffer](USoundWaveProcedural* InWave, int32 SamplesRequired)
		{
			InWave->QueueAudio(reinterpret_cast<const uint8*>(Buffer->GetData()), Buffer->Num() * sizeof(int16));
		});
		Wave->QueueAudio(reinterpret_cast<const uint8*>(Buffer->GetData()), Buffer->Num() * sizeof(int16));
		Sound = Wave;
	}

	UAudioComponent* AC = NewObject<UAudioComponent>(AttachTo->GetOwner());
	AC->bAutoActivate = false;
	AC->bAutoDestroy = false;
	AC->AttenuationSettings = Attenuation;
	AC->SetupAttachment(AttachTo);
	AC->SetSound(Sound);
	AC->SetVolumeMultiplier(Settings->EngineVolume);
	AC->RegisterComponent();
	AC->Play();
	return AC;
}
