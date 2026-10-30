#pragma once

#include "core/Constants.h"
#include "core/Math.h"

namespace dalia::dsp {
	static void MixToBuffer(float* DALIA_RESTRICT targetBuffer, const float* DALIA_RESTRICT buffer,
		uint32_t sampleCount);

	static void ApplyGainAndFade(float* DALIA_RESTRICT buffer, uint32_t frameCount, uint32_t channels,
		float& currentGain, float targetGain, float smoothingCoefficient,
		float& currentFade, float targetFade, float fadeStep);

	// Applies a linear fade over a block of frames
	static void ApplyBlockFade(
		float* DALIA_RESTRICT buffer,
		uint32_t frameCount,
		uint32_t channels,
		float& currentFade,
		float targetFade,
		float fadeStep);

	// Applies a cross-fade between a dry and wet buffer over a block of frames
	inline void ApplyBlockCrossFade(
		float* DALIA_RESTRICT inOutDryBuffer,
		const float* DALIA_RESTRICT inWetBuffer,
		uint32_t frameCount,
		uint32_t channels,
		float& currentMix,
		float targetMix,
		float mixDelta);

	inline void MixVoiceBlock(
		const float* DALIA_RESTRICT inPtr,
		float* DALIA_RESTRICT outPtr,
		uint32_t framesToMix,
		uint32_t sourceChannels,
		uint32_t outChannels,
		float& currentFadeGain,
		float targetFadeGain,
		float fadeStep,
		float currentGainMatrix[CHANNELS_MAX][CHANNELS_MAX],
		const float targetGainMatrix[CHANNELS_MAX][CHANNELS_MAX],
		float smoothingCoefficient);
}
