#include "mixer/effects/Biquad.h"

#include "core/Constants.h"

#include <cmath>
#include <algorithm>

#include "core/Math.h"

namespace dalia {

	void BiquadParams::Sanitize() {
		frequency = std::clamp(frequency, FILTER_FREQUENCY_MIN, FILTER_FREQUENCY_MAX);
		resonance = std::clamp(resonance, FILTER_RESONANCE_MIN, FILTER_RESONANCE_MAX);
	}

	void Biquad::SetParams(const BiquadParams& params) {
		frequencySlew.SetTarget(params.frequency);
		resonanceSlew.SetTarget(params.resonance);

		if (type != params.type) {
			type = params.type;
			ClearDelayLines();
			forceRecalculate = true;
		}

		if (isFirstProcess) {
			frequencySlew.SnapToTarget();
			resonanceSlew.SnapToTarget();
		}
	}

	void Biquad::Process(float* buffer, uint32_t frameCount, uint32_t channels, uint32_t sampleRate) {
		if (isFirstProcess) {
			CalculateCoefficients(sampleRate);
			isFirstProcess = false;
		}

		for (uint32_t i = 0; i < frameCount; i += CONTROL_RATE) {
            uint32_t framesThisChunk = std::min(CONTROL_RATE, frameCount - i);

            bool paramsChanged = forceRecalculate;
			paramsChanged |= frequencySlew.Process();
			paramsChanged |= resonanceSlew.Process();

            if (paramsChanged) {
	            CalculateCoefficients(sampleRate);
            	forceRecalculate = false;
            }

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

	void Biquad::Flush() {
		isFirstProcess = true;
		ClearDelayLines();
	}

	void Biquad::Reset() {
		isFirstProcess = true;
		forceRecalculate = false;

		type = BiquadParams::Type::LowPass;
		frequencySlew = SlewFloat{};
		resonanceSlew = SlewFloat{};

		b0 = 1.0, b1 = 0.0, b2 = 0.0;
		a1 = 0.0, a2 = 0.0;

		Flush();
	}

	void Biquad::CalculateCoefficients(uint32_t sampleRate) {
		float fSampleRate = static_cast<float>(sampleRate);
		float nyquist = fSampleRate * 0.5f; // Nyquist limit
		float frequency = std::clamp(frequencySlew.current, 10.0f, nyquist - 10.0f);
		float q = std::max(resonanceSlew.current, 0.1f);

		// RBJ variables
		double omega = 2.0 * dPI * frequency / fSampleRate;
		double sn = std::sin(omega);
		double cs = std::cos(omega);
		double alpha = sn / (2.0 * q);

		double a0 = 1.0;

		switch (type) {
			case BiquadParams::Type::LowPass: {
				b0 = (1.0 - cs) * 0.5;
				b1 = 1.0 - cs;
				b2 = (1.0 - cs) * 0.5;
				a0       = 1.0 + alpha;
				a1 = -2.0 * cs;
				a2 = 1.0 - alpha;
				break;
			}
			case BiquadParams::Type::HighPass: {
				b0 = (1.0 + cs) * 0.5;
				b1 = -(1.0 + cs);
				b2 = (1.0 + cs) * 0.5;
				a0       = 1.0 + alpha;
				a1 = -2.0 * cs;
				a2 = 1.0 - alpha;
				break;
			}
			case BiquadParams::Type::BandPass: {
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

	void Biquad::ClearDelayLines() {
		std::memset(z1, 0, sizeof(z1));
		std::memset(z2, 0, sizeof(z2));
	}
}
