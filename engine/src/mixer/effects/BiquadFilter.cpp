#include "mixer/effects/BiquadFilter.h"

#include "core/Constants.h"

#include <cmath>
#include <algorithm>

#include "core/Math.h"

namespace dalia {

	void BiquadConfig::Sanitize() {
		frequency = std::clamp(frequency, FILTER_FREQUENCY_MIN, FILTER_FREQUENCY_MAX);
		resonance = std::clamp(resonance, FILTER_RESONANCE_MIN, FILTER_RESONANCE_MAX);
	}

	void BiquadFilter::SetConfig(const BiquadConfig& config) {
		targetType = config.type;
		targetFrequency = config.frequency;
		targetResonance = config.resonance;

		if (isFirstProcess) {
			currentType = targetType;
			currentFrequency = targetFrequency;
			currentResonance = targetResonance;
		}
	}

	void BiquadFilter::Process(float* buffer, uint32_t frameCount, uint32_t channels, float sampleRate) {
		if (isFirstProcess) {
			CalculateCoefficients(sampleRate);
			isFirstProcess = false;
		}

		for (uint32_t i = 0; i < frameCount; i += CONTROL_RATE) {
            uint32_t framesThisChunk = std::min(CONTROL_RATE, frameCount - i);

            bool paramsChanged = false;

			if (currentType != targetType) {
				paramsChanged = true;
				currentType = targetType;

				ClearDelayLines(); // Clear to avoid popping
			}

            // 1. Frequency Smoothing
            if (!math::NearlyEqual(currentFrequency, targetFrequency, EPSILON_FREQUENCY)) {
                // We are far away, smoothly interpolate
                currentFrequency += (targetFrequency - currentFrequency) * 0.05f;
                paramsChanged = true;
            }
            else if (currentFrequency != targetFrequency) {
                currentFrequency = targetFrequency;
                paramsChanged = true;
            }

            // 2. Resonance (Q) Smoothing
            if (!math::NearlyEqual(currentResonance, targetResonance, EPSILON_RESONANCE)) {
                currentResonance += (targetResonance - currentResonance) * 0.05f;
                paramsChanged = true;
            }
            else if (currentResonance != targetResonance) {
                currentResonance = targetResonance;
                paramsChanged = true;
            }

            if (paramsChanged) CalculateCoefficients(sampleRate);

            for (uint32_t j = 0; j < framesThisChunk; j++) {
                uint32_t frameIndex = i + j;

                for (uint32_t channel = 0; channel < channels; channel++) {
                    uint32_t sampleIndex = (frameIndex * channels) + channel;
                    auto x = static_cast<double>(buffer[sampleIndex]); // Sample

                    double y = (b0 * x) + z1[channel];
                    z1[channel] = (b1 * x) - (a1 * y) + z2[channel];
                    z2[channel] = (b2 * x) - (a2 * y);

                    buffer[sampleIndex] = static_cast<float>(y);
                }
            }
        }
    }

	void BiquadFilter::Flush() {
		isFirstProcess = true;
		ClearDelayLines();
	}

	void BiquadFilter::Reset() {
		isFirstProcess = true;

		targetType = BiquadFilterType::LowPass;
		currentType = BiquadFilterType::LowPass;
		targetFrequency = 20000.0f;
		currentFrequency = 20000.0f;
		targetResonance = 0.707f;
		currentResonance = 0.707f;

		b0 = 1.0, b1 = 0.0, b2 = 0.0;
		a1 = 0.0, a2 = 0.0;

		Flush();
	}

	void BiquadFilter::CalculateCoefficients(float sampleRate) {
		float nyquist = sampleRate * 0.5f; // Nyquist limit (sample rate cannot exceed half the sample rate
		float frequency = std::clamp(currentFrequency, 10.0f, nyquist - 10.0f);
		float q = std::max(currentResonance, 0.1f);

		// RBJ variables
		double omega = 2.0 * dPI * frequency / sampleRate;
		double sn = std::sin(omega);
		double cs = std::cos(omega);
		double alpha = sn / (2.0 * q);

		double a0 = 1.0;

		switch (currentType) {
			case BiquadFilterType::LowPass: {
				b0 = (1.0 - cs) * 0.5;
				b1 = 1.0 - cs;
				b2 = (1.0 - cs) * 0.5;
				a0       = 1.0 + alpha;
				a1 = -2.0 * cs;
				a2 = 1.0 - alpha;
				break;
			}
			case BiquadFilterType::HighPass: {
				b0 = (1.0 + cs) * 0.5;
				b1 = -(1.0 + cs);
				b2 = (1.0 + cs) * 0.5;
				a0       = 1.0 + alpha;
				a1 = -2.0 * cs;
				a2 = 1.0 - alpha;
				break;
			}
			case BiquadFilterType::BandPass: {
				b0 = alpha;
				b1 = 0.0;
				b2 = -alpha;
				a0       = 1.0 + alpha;
				a1 = -2.0 * cs;
				a2 = 1.0 - alpha;
				break;
			}
		}

		double a0Inverse = 1.0 / a0;
		b0 *= a0Inverse;
		b1 *= a0Inverse;
		b2 *= a0Inverse;
		a1 *= a0Inverse;
		a2 *= a0Inverse;
	}

	void BiquadFilter::ClearDelayLines() {
		std::memset(z1, 0, sizeof(z1));
		std::memset(z2, 0, sizeof(z2));
	}
}
