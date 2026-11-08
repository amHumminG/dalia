#pragma once
#include "mixer/dsp/Gain.h"
#include "core/Constants.h"
#include "core/Math.h"

#include <cstring>

namespace dalia::dsp {

	inline void MixToBuffer(float* DALIA_RESTRICT targetBuffer, const float* DALIA_RESTRICT buffer,
		uint32_t sampleCount) {
		for (uint32_t i = 0; i < sampleCount; i++) {
			targetBuffer[i] += buffer[i];
		}
	}

	inline void ApplyGainAndFade(float* DALIA_RESTRICT buffer, uint32_t frameCount, uint32_t channels,
		float& currentGain, float targetGain, float smoothingCoefficient,
		float& currentFade, float targetFade, float fadeStep) {

		bool gainStable = math::NearlyEqual(currentGain, targetGain, EPSILON_GAIN);
		bool fadeStable = math::NearlyEqual(currentFade, targetFade, EPSILON_GAIN);

		// Fast path
		if (gainStable && fadeStable) {
			currentGain = targetGain;
			currentFade = targetFade;

			float combinedGain = currentGain * currentFade;

			if (math::NearlyEqual(combinedGain, 1.0f, EPSILON_GAIN)) return;

			uint32_t sampleCount = frameCount * channels;
			if (math::NearlyEqual(combinedGain, 0.0f, EPSILON_GAIN)) {
				std::memset(buffer, 0, sampleCount * sizeof(float));
				return;
			}

			for (uint32_t s = 0; s < sampleCount; s++) buffer[s] *= combinedGain;
			return;
		}

		// Slow path
		for (uint32_t f = 0; f < frameCount; f++) {
			if (!gainStable) currentGain += (targetGain - currentGain) * smoothingCoefficient;
			if (!fadeStable) StepFadeGain(currentFade, targetFade, fadeStep);

			float combinedGain = currentGain * currentFade;
			for (uint32_t c = 0; c < channels; c++) buffer[(f * channels) + c] *= combinedGain;
		}
	}

	// Applies a linear fade over a block of frames
	inline void ApplyBlockFade(
		float* DALIA_RESTRICT buffer,
		uint32_t frameCount,
		uint32_t channels,
		float& currentFade,
		float targetFade,
		float fadeStep) {

		// Fast path (finished fade)
		if (math::NearlyEqual(currentFade, targetFade, EPSILON_GAIN)) {
			currentFade = targetFade;

			if (math::NearlyEqual(currentFade, GAIN_DEFAULT, EPSILON_GAIN)) {
				return; // 1.0f gain (nothing to do)
			}

			uint32_t sampleCount = frameCount * channels;
			if (math::NearlyEqual(currentFade, GAIN_SILENCE, EPSILON_GAIN)) {
				std::memset(buffer, 0, sampleCount * sizeof(float));
				return; // 0.0f gain
			}

			for (uint32_t sample = 0; sample < sampleCount; sample++) buffer[sample] *= currentFade;
			return;
		}

		// Slow path (active fade)
		for (uint32_t frame = 0; frame < frameCount; frame++) {
			StepFadeGain(currentFade, targetFade, fadeStep);

			for (uint32_t channel = 0; channel < channels; channel++) {
				buffer[(frame * channels) + channel] *= currentFade;
			}
		}
	}

	// Applies a cross-fade between a dry and wet buffer over a block of frames
	inline void ApplyBlockCrossFade(
		float* DALIA_RESTRICT inOutDryBuffer,
		const float* DALIA_RESTRICT inWetBuffer,
		uint32_t frameCount,
		uint32_t channels,
		float& currentMix,
		float targetMix,
		float mixDelta) {

		// Fast path (finished fade)
		if (math::NearlyEqual(currentMix, targetMix, EPSILON_GAIN)) {
			currentMix = targetMix;
			uint32_t sampleCount = frameCount * channels;

			if (math::NearlyEqual(currentMix, GAIN_SILENCE, EPSILON_GAIN)) {
				return; // 100% dry
			}
			if (math::NearlyEqual(currentMix, GAIN_DEFAULT, EPSILON_GAIN)) {
				std::memcpy(inOutDryBuffer, inWetBuffer, sampleCount * sizeof(float));
				return; // 100% wet
			}
		}

		// Slow path (active cross-fade)
		for (uint32_t frame = 0; frame < frameCount; frame++) {
			StepFadeGain(currentMix, targetMix, mixDelta);

			for (uint32_t channel = 0; channel < channels; channel++) {
				uint32_t index = (frame * channels) + channel;
				float dry = inOutDryBuffer[index];
				float wet = inWetBuffer[index];

				inOutDryBuffer[index] = dry + currentMix * (wet - dry);
			}
		}
	}

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
		float smoothingCoefficient) {

		// Load local variables
		float localFadeGain = currentFadeGain;
		float localGainMatrix[CHANNELS_MAX][CHANNELS_MAX];
		std::memcpy(localGainMatrix, currentGainMatrix, CHANNELS_MAX * CHANNELS_MAX * sizeof(float));

		for (uint32_t i = 0; i < framesToMix; i++) {
			StepFadeGain(localFadeGain, targetFadeGain, fadeStep);
			StepMatrixGains(localGainMatrix, targetGainMatrix, sourceChannels, outChannels, smoothingCoefficient);

			for (uint32_t inC = 0; inC < sourceChannels; inC++) {
				float sample = inPtr[inC] * localFadeGain;

				for (uint32_t outC = 0; outC < outChannels; outC++) {
					outPtr[outC] += sample * localGainMatrix[inC][outC];
				}
			}

			inPtr += sourceChannels;
			outPtr += outChannels;
		}

		currentFadeGain = localFadeGain;
		std::memcpy(currentGainMatrix, localGainMatrix, CHANNELS_MAX * CHANNELS_MAX * sizeof(float));
	}
}
