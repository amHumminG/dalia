#pragma once
#include "dalia/audio/EffectControl.h"
#include "core/Constants.h"
#include <cstdint>
#include <cstring>

namespace dalia {

    struct BiquadFilter {
    	bool isFirstProcess = true;

    	BiquadFilterType targetType = BiquadFilterType::LowPass;
        BiquadFilterType currentType = BiquadFilterType::LowPass;
        float targetFrequency = 20000.0f;
        float currentFrequency = 20000.0f;
        float targetResonance = 0.707f;
        float currentResonance = 0.707f;

        double b0 = 1.0, b1 = 0.0, b2 = 0.0;
        double a1 = 0.0, a2 = 0.0;

        double z1[CHANNELS_MAX] = {0.0};
        double z2[CHANNELS_MAX] = {0.0};

		void SetConfig(const BiquadConfig& config);
    	void Process(float* buffer, uint32_t frameCount, uint32_t channels, float sampleRate);
    	void Flush(); // Clear history
        void Reset();

    private:
    	void CalculateCoefficients(float sampleRate);
    	void ClearDelayLines();

    };

}
