#pragma once
#include "dalia/audio/EffectControl.h"
#include "core/Constants.h"
#include <cstdint>
#include <cstring>

namespace dalia {

    struct BiquadFilter {
        uint32_t generation = START_GENERATION;
        BiquadFilterType type = BiquadFilterType::LowPass;

        float targetFrequency = 20000.0f;
        float currentFrequency = 20000.0f;

        float targetResonance = 0.707f;
        float currentResonance = 0.707f;

        double b0 = 1.0, b1 = 0.0, b2 = 0.0;
        double a1 = 0.0, a2 = 0.0;

        double z1[CHANNELS_MAX] = {0.0};
        double z2[CHANNELS_MAX] = {0.0};

        // Clears the internal delay lines
        void Flush() {
            std::memset(z1, 0, sizeof(z1));
            std::memset(z2, 0, sizeof(z2));
        }

        void Reset() {
            generation++;
            type = BiquadFilterType::LowPass;

            targetFrequency = 20000.0f;
            currentFrequency = 20000.0f;

            targetResonance = 0.707f; // Resonance
            currentResonance = 0.707f;

            b0 = 1.0, b1 = 0.0, b2 = 0.0;
            a1 = 0.0, a2 = 0.0;

            Flush();
        }
    };

    void CalculateBiquadCoefficients(BiquadFilter& state, float sampleRate);
    void ProcessBiquad(float* buffer, uint32_t frameCount, uint32_t channels, BiquadFilter& state, float sampleRate);
}
