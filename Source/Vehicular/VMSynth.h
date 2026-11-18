#pragma once

#include "CoreMinimal.h"
#include "VMTypes.h"

/**
 * Tiny offline synthesiser: builds every sound effect from oscillators and filtered noise,
 * so the game has audio without shipping any sound assets.
 */
namespace VMSynth
{
	constexpr int32 SampleRate = 22050;

	/** 16-bit mono PCM for a one-shot effect. */
	TArray<int16> Generate(EVMSound Sound);

	/** Exactly one second of seamlessly looping engine drone (fundamental 50 Hz). */
	TArray<int16> EngineLoop();

	/** Base pitch of EngineLoop() in Hz, for mapping RPM to a pitch multiplier. */
	constexpr float EngineBaseHz = 50.f;
}
