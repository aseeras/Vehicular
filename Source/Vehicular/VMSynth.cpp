#include "VMSynth.h"

namespace
{
	using namespace VMSynth;

	enum class EWave : uint8 { Sine, Square, Saw, Triangle };

	float Osc(EWave Wave, float Phase)
	{
		const float P = Phase - FMath::FloorToFloat(Phase);
		switch (Wave)
		{
		case EWave::Square:   return P < 0.5f ? 1.f : -1.f;
		case EWave::Saw:      return 2.f * P - 1.f;
		case EWave::Triangle: return 1.f - 4.f * FMath::Abs(P - 0.5f);
		default:              return FMath::Sin(2.f * PI * P);
		}
	}

	struct FMix
	{
		TArray<float> Samples;
		FRandomStream Rng;

		explicit FMix(float Seconds, int32 Seed = 7)
			: Rng(Seed)
		{
			Samples.SetNumZeroed(FMath::CeilToInt(Seconds * SampleRate));
		}

		/** A pitched note with a 5ms attack and exponential decay. Frequency glides from F0 to F1. */
		void Tone(float Start, float Duration, float F0, float F1, float Amp, EWave Wave, float Decay, float VibratoHz = 0.f, float VibratoDepth = 0.f)
		{
			const int32 Begin = FMath::RoundToInt(Start * SampleRate);
			const int32 Count = FMath::RoundToInt(Duration * SampleRate);
			float Phase = 0.f;
			for (int32 i = 0; i < Count && Begin + i < Samples.Num(); ++i)
			{
				const float T = static_cast<float>(i) / SampleRate;
				float Freq = FMath::Lerp(F0, F1, T / Duration);
				if (VibratoDepth > 0.f)
				{
					Freq *= 1.f + VibratoDepth * FMath::Sin(2.f * PI * VibratoHz * T);
				}
				Phase += Freq / SampleRate;
				const float Env = FMath::Min(1.f, T / 0.005f) * FMath::Exp(-T * Decay);
				Samples[Begin + i] += Osc(Wave, Phase) * Amp * Env;
			}
		}

		/** Low-pass filtered noise; Cutoff is a one-pole coefficient (0..1) gliding from C0 to C1. */
		void Noise(float Start, float Duration, float Amp, float C0, float C1, float Decay, bool bSwell = false)
		{
			const int32 Begin = FMath::RoundToInt(Start * SampleRate);
			const int32 Count = FMath::RoundToInt(Duration * SampleRate);
			float Y = 0.f;
			for (int32 i = 0; i < Count && Begin + i < Samples.Num(); ++i)
			{
				const float T = static_cast<float>(i) / SampleRate;
				const float Alpha = FMath::Lerp(C0, C1, T / Duration);
				Y += Alpha * (Rng.FRandRange(-1.f, 1.f) - Y);
				float Env = FMath::Min(1.f, T / 0.003f) * FMath::Exp(-T * Decay);
				if (bSwell)
				{
					// Rise then fall: a whoosh.
					Env = FMath::Sin(PI * T / Duration);
				}
				Samples[Begin + i] += Y * Amp * Env * 2.f;
			}
		}

		TArray<int16> ToPCM(float Gain = 1.f) const
		{
			TArray<int16> Out;
			Out.SetNumUninitialized(Samples.Num());
			const int32 FadeOut = FMath::Min(Samples.Num(), SampleRate / 100);
			for (int32 i = 0; i < Samples.Num(); ++i)
			{
				float V = FMath::Tanh(Samples[i] * Gain); // soft clip
				if (i >= Samples.Num() - FadeOut)
				{
					V *= static_cast<float>(Samples.Num() - i) / FadeOut; // avoid an end click
				}
				Out[i] = static_cast<int16>(FMath::Clamp(V * 32000.f, -32767.f, 32767.f));
			}
			return Out;
		}
	};

	void Notes(FMix& Mix, std::initializer_list<float> Freqs, float Step, float NoteLen, float Amp, EWave Wave, float Decay)
	{
		float T = 0.f;
		for (float F : Freqs)
		{
			Mix.Tone(T, NoteLen, F, F, Amp, Wave, Decay);
			T += Step;
		}
	}
}

TArray<int16> VMSynth::Generate(EVMSound Sound)
{
	switch (Sound)
	{
	case EVMSound::ImpactLight:
	{
		FMix M(0.3f, 11);
		M.Noise(0.f, 0.25f, 0.8f, 0.35f, 0.2f, 18.f);
		M.Tone(0.f, 0.2f, 90.f, 50.f, 0.7f, EWave::Sine, 15.f);
		return M.ToPCM();
	}
	case EVMSound::ImpactHeavy:
	{
		FMix M(0.7f, 12);
		M.Noise(0.f, 0.6f, 1.f, 0.3f, 0.1f, 7.f);
		M.Tone(0.f, 0.5f, 70.f, 35.f, 0.9f, EWave::Sine, 6.f);
		M.Tone(0.f, 0.4f, 420.f, 400.f, 0.15f, EWave::Square, 10.f);
		M.Tone(0.02f, 0.35f, 611.f, 590.f, 0.1f, EWave::Square, 12.f);
		return M.ToPCM(1.2f);
	}
	case EVMSound::Explosion:
	{
		FMix M(1.8f, 13);
		M.Noise(0.f, 1.7f, 1.f, 0.18f, 0.03f, 2.5f);
		M.Tone(0.f, 1.2f, 55.f, 25.f, 1.f, EWave::Sine, 3.f);
		M.Noise(0.1f, 0.6f, 0.4f, 0.6f, 0.4f, 6.f);
		return M.ToPCM(1.5f);
	}
	case EVMSound::Squash:
	{
		FMix M(0.35f, 14);
		M.Noise(0.f, 0.3f, 0.7f, 0.2f, 0.08f, 14.f);
		M.Tone(0.f, 0.25f, 180.f, 60.f, 0.6f, EWave::Sine, 10.f);
		return M.ToPCM();
	}
	case EVMSound::Yelp:
	{
		FMix M(0.6f, 15);
		M.Tone(0.f, 0.08f, 500.f, 760.f, 0.3f, EWave::Saw, 0.f, 9.f, 0.03f);
		M.Tone(0.08f, 0.47f, 760.f, 420.f, 0.3f, EWave::Saw, 2.5f, 7.f, 0.05f);
		return M.ToPCM(0.9f);
	}
	case EVMSound::Pickup:
	{
		FMix M(0.35f);
		Notes(M, { 523.f, 659.f, 784.f, 1046.f }, 0.06f, 0.12f, 0.25f, EWave::Square, 12.f);
		return M.ToPCM();
	}
	case EVMSound::Checkpoint:
	{
		FMix M(0.45f);
		M.Tone(0.f, 0.3f, 880.f, 880.f, 0.5f, EWave::Sine, 8.f);
		M.Tone(0.08f, 0.35f, 1320.f, 1320.f, 0.4f, EWave::Sine, 7.f);
		return M.ToPCM();
	}
	case EVMSound::CountdownBeep:
	{
		FMix M(0.2f);
		M.Tone(0.f, 0.18f, 440.f, 440.f, 0.6f, EWave::Sine, 10.f);
		M.Tone(0.f, 0.18f, 440.f, 440.f, 0.08f, EWave::Square, 10.f);
		return M.ToPCM();
	}
	case EVMSound::Go:
	{
		FMix M(0.6f);
		M.Tone(0.f, 0.55f, 880.f, 880.f, 0.6f, EWave::Sine, 4.f);
		M.Tone(0.f, 0.55f, 880.f, 880.f, 0.1f, EWave::Square, 4.f);
		return M.ToPCM();
	}
	case EVMSound::EMP:
	{
		FMix M(0.7f, 16);
		M.Tone(0.f, 0.6f, 1600.f, 80.f, 0.35f, EWave::Square, 4.f);
		M.Noise(0.f, 0.4f, 0.3f, 0.6f, 0.3f, 10.f);
		return M.ToPCM();
	}
	case EVMSound::Oil:
	{
		FMix M(0.4f, 17);
		M.Noise(0.f, 0.35f, 0.8f, 0.1f, 0.05f, 9.f);
		M.Tone(0.f, 0.3f, 120.f, 70.f, 0.5f, EWave::Sine, 12.f);
		return M.ToPCM();
	}
	case EVMSound::Nitro:
	{
		FMix M(0.9f, 18);
		M.Noise(0.f, 0.9f, 0.8f, 0.05f, 0.5f, 0.f, true);
		M.Tone(0.f, 0.9f, 120.f, 240.f, 0.2f, EWave::Saw, 2.f);
		return M.ToPCM();
	}
	case EVMSound::Recover:
	{
		FMix M(0.5f, 19);
		M.Noise(0.f, 0.45f, 0.5f, 0.1f, 0.4f, 0.f, true);
		M.Tone(0.f, 0.3f, 300.f, 600.f, 0.2f, EWave::Triangle, 6.f);
		return M.ToPCM();
	}
	case EVMSound::Purchase:
	{
		FMix M(0.3f);
		Notes(M, { 1318.f, 1760.f }, 0.07f, 0.15f, 0.3f, EWave::Square, 10.f);
		return M.ToPCM();
	}
	case EVMSound::Denied:
	{
		FMix M(0.35f);
		M.Tone(0.f, 0.3f, 110.f, 110.f, 0.35f, EWave::Square, 6.f);
		M.Tone(0.f, 0.3f, 104.f, 104.f, 0.3f, EWave::Square, 6.f);
		return M.ToPCM();
	}
	case EVMSound::MenuMove:
	{
		FMix M(0.06f);
		M.Tone(0.f, 0.05f, 1000.f, 1000.f, 0.2f, EWave::Square, 40.f);
		return M.ToPCM();
	}
	case EVMSound::Fanfare:
	{
		FMix M(1.2f);
		Notes(M, { 523.f, 659.f, 784.f }, 0.12f, 0.14f, 0.25f, EWave::Square, 8.f);
		M.Tone(0.36f, 0.8f, 1046.f, 1046.f, 0.3f, EWave::Square, 2.5f);
		M.Tone(0.36f, 0.8f, 262.f, 262.f, 0.4f, EWave::Sine, 2.5f);
		return M.ToPCM();
	}
	case EVMSound::Defeat:
	{
		FMix M(1.5f);
		Notes(M, { 392.f, 370.f, 349.f }, 0.25f, 0.28f, 0.22f, EWave::Saw, 3.f);
		M.Tone(0.75f, 0.7f, 262.f, 250.f, 0.25f, EWave::Saw, 2.f, 5.f, 0.02f);
		return M.ToPCM();
	}
	default:
		return TArray<int16>();
	}
}

TArray<int16> VMSynth::EngineLoop()
{
	// One second at integer-Hz partials, so the buffer loops without a seam.
	FMix M(1.f, 21);
	const int32 N = M.Samples.Num();
	float Y = 0.f;
	for (int32 i = 0; i < N; ++i)
	{
		const float T = static_cast<float>(i) / SampleRate;
		float V = 0.f;
		for (int32 K = 1; K <= 6; ++K)
		{
			V += FMath::Sin(2.f * PI * EngineBaseHz * K * T + K * 0.7f) / K;
		}
		// Firing pulses: amplitude wobble at half the fundamental.
		V *= 0.75f + 0.25f * FMath::Sin(2.f * PI * (EngineBaseHz * 0.5f) * T);
		Y += 0.15f * (M.Rng.FRandRange(-1.f, 1.f) - Y);
		M.Samples[i] = V * 0.35f + Y * 0.25f;
	}
	TArray<int16> Out;
	Out.SetNumUninitialized(N);
	for (int32 i = 0; i < N; ++i)
	{
		Out[i] = static_cast<int16>(FMath::Clamp(FMath::Tanh(M.Samples[i]) * 30000.f, -32767.f, 32767.f));
	}
	return Out;
}
