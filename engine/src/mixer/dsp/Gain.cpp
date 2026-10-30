#include "mixer/dsp/Gain.h"

#include <cmath>

namespace dalia::dsp {

	void StepMatrixGains(float(* current)[8], const float(* target)[8], uint32_t inChannels, uint32_t outChannels,
		float smoothingCoefficient) {
		for (uint32_t inC = 0; inC < inChannels; inC++) {
			for (uint32_t outC = 0; outC < outChannels; outC++) {
				float diff = target[inC][outC] - current[inC][outC];

				if (std::abs(diff) < EPSILON_GAIN) current[inC][outC] = target[inC][outC];
				else current[inC][outC] += diff * smoothingCoefficient;
			}
		}
	}

	float CalculateLinearFadeStep(float fadeTimeSeconds, uint32_t sampleRate) {
		return 1.0f / (fadeTimeSeconds * static_cast<float>(sampleRate));
	}

	void StepFadeGain(float& currentGain, float targetGain, float stepSize) {
		if (currentGain == targetGain) return;

		if (currentGain > targetGain) {
			// Fading down
			currentGain -= stepSize;
			if (currentGain < targetGain) currentGain = targetGain;
		}
		else {
			// Fading up
			currentGain += stepSize;
			if (currentGain > targetGain) currentGain = targetGain;
		}
	}
}
