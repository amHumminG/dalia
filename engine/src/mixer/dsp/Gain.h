#pragma once

#include "core/Constants.h"

namespace dalia::dsp {

	static void StepMatrixGains(
		float (* DALIA_RESTRICT current)[CHANNELS_MAX],
		const float (* DALIA_RESTRICT target)[CHANNELS_MAX],
		uint32_t inChannels,
		uint32_t outChannels,
		float smoothingCoefficient);

	static float CalculateLinearFadeStep(float fadeTimeSeconds, uint32_t sampleRate);

	static void StepFadeGain(float& currentGain, float targetGain, float stepSize);
}
