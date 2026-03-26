#include "mixer/effects/Biquad.h"

#include "core/Constants.h"

#include <cmath>
#include <algorithm>

#include "core/Math.h"

namespace dalia {

    void CalculateBiquadCoefficients(Biquad& state, float sampleRate) {
        float nyquist = sampleRate * 0.5f; // Nyquist limit (sample rate cannot exceed half the sample rate
        float frequency = std::clamp(state.currentFrequency, 10.0f, nyquist - 10.0f);
        float q = std::max(state.currentResonance, 0.1f);

        // RBJ variables
        const float omega = 2.0f * PI * frequency / sampleRate;
        const float sn = std::sin(omega);
        const float cs = std::cos(omega);
        const float alpha = sn / (2.0f * q);

        float a0 = 1.0f;

        switch (state.type) {
            case BiquadFilterType::LowPass: {
                state.b0 = (1.0f - cs) * 0.5f;
                state.b1 = 1.0f - cs;
                state.b2 = (1.0f - cs) * 0.5f;
                a0       = 1.0f + alpha;
                state.a1 = -2.0f * cs;
                state.a2 = 1.0f - alpha;
                break;
            }
            case BiquadFilterType::HighPass: {
                state.b0 = (1.0f + cs) * 0.5f;
                state.b1 = -(1.0f - cs);
                state.b2 = (1.0f + cs) * 0.5f;
                a0       = 1.0f + alpha;
                state.a1 = -2.0f * cs;
                state.a2 = 1.0f - alpha;
                break;
            }
            case BiquadFilterType::BandPass: {
                state.b0 = alpha;
                state.b1 = 0.0f;
                state.b2 = -alpha;
                a0       = 1.0f + alpha;
                state.a1 = -2.0f * cs;
                state.a2 = 1.0f - alpha;
                break;
            }
        }

        const float a0Inverse = 1.0f / a0;
        state.b0 *= a0Inverse;
        state.b1 *= a0Inverse;
        state.b2 *= a0Inverse;
        state.a1 *= a0Inverse;
        state.a2 *= a0Inverse;
    }

    void ProcessBiquad(float* buffer, uint32_t frameCount, uint32_t channels, Biquad& state, float sampleRate) {
        for (uint32_t i = 0; i < frameCount; i += CONTROL_RATE) {
            uint32_t framesThisChunk = std::min(CONTROL_RATE, frameCount - i);

            bool paramsChanged = false;

            // 1. Frequency Smoothing
            if (!NearlyEqual(state.currentFrequency, state.targetFrequency, FREQUENCY_EPSILON)) {
                // We are far away, smoothly interpolate
                state.currentFrequency += (state.targetFrequency - state.currentFrequency) * 0.05f;
                paramsChanged = true;
            }
            else if (state.currentFrequency != state.targetFrequency) {
                state.currentFrequency = state.targetFrequency;
                paramsChanged = true;
            }

            // 2. Resonance (Q) Smoothing
            if (!NearlyEqual(state.currentResonance, state.targetResonance, RESONANCE_EPSILON)) {
                state.currentResonance += (state.targetResonance - state.currentResonance) * 0.05f;
                paramsChanged = true;
            }
            else if (state.currentResonance != state.targetResonance) {
                state.currentResonance = state.targetResonance;
                paramsChanged = true;
            }

            if (paramsChanged) CalculateBiquadCoefficients(state, sampleRate);

            for (uint32_t j = 0; j < framesThisChunk; j++) {
                uint32_t frameIndex = i + j;

                for (uint32_t channel = 0; channel < channels; channel++) {
                    uint32_t sampleIndex = (frameIndex * channels) + channel;
                    float sample = buffer[sampleIndex];

                    float y = (state.b0 * sample) + state.z1[channel];
                    state.z1[channel] = (state.b1 * sample) - (state.a1 * y) + state.z2[channel];
                    state.z2[channel] = (state.b2 * sample) - (state.a2 * y);

                    buffer[sampleIndex] = y;
                }
            }
        }
    }
}
