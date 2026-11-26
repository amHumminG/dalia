#include "mixer/dsp/PeakLimiter.hpp"

#include <cmath>
#include <algorithm>

void PeakLimiter::Initialize(float sampleRate, float attackMs, float releaseMs) {
	attackCoeff  = std::exp(-1.0f / ((attackMs * 0.001f) * sampleRate));
	releaseCoeff = std::exp(-1.0f / ((releaseMs * 0.001f) * sampleRate));
}

void PeakLimiter::ProcessBuffer(float* buffer, uint32_t frameCount, uint32_t channels) {
	for (uint32_t i = 0; i < frameCount; i++) {
		uint32_t frameIndex = i * channels;

		// Find the loudest peak among all channels in this frame
		float maxPeak = 0.0f;
		for (uint32_t c = 0; c < channels; c++) {
			maxPeak = std::max(maxPeak, std::abs(buffer[frameIndex + c]));
		}

		// Track peak volume
		if (maxPeak > envelope) {
			envelope = attackCoeff * envelope + (1.0f - attackCoeff) * maxPeak;
		}
		else {
			envelope = releaseCoeff * envelope + (1.0f - releaseCoeff) * maxPeak;
		}

		// Calculate gain reduction
		float targetGain = 1.0f;
		if (envelope > threshold) targetGain = threshold / envelope;

		// Smooth gain application
		if (targetGain < currentGain) {
			currentGain = attackCoeff * currentGain + (1.0f - attackCoeff) * targetGain;
		}
		else {
			currentGain = releaseCoeff * currentGain + (1.0f - releaseCoeff) * targetGain;
		}

		// Apply gain
		for (uint32_t c = 0; c < channels; c++) buffer[frameIndex + c] *= currentGain;
	}
}
