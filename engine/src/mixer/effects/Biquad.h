#pragma once

#include "dalia/audio/EffectControl.h"
#include "core/Constants.h"
#include "dsp/Slew.h"

#include <cstdint>

namespace dalia {

    struct Biquad {
    	uint32_t gen = NO_GENERATION;
    	bool isFirstProcess = true;
    	bool forceRecalculate = false; // Set true if type changes

    	BiquadParams::Type type = BiquadParams::Type::LowPass;
    	dsp::SlewFloat frequencySlew;
    	dsp::SlewFloat resonanceSlew;

    	double b0 = 1.0, b1 = 0.0, b2 = 0.0;
    	double a1 = 0.0, a2 = 0.0;
    	double z1[CHANNELS_MAX] = {0.0};
    	double z2[CHANNELS_MAX] = {0.0};

		void SetParams(const BiquadParams& params);
    	void Process(float* buffer, uint32_t frameCount, uint32_t channels, uint32_t sampleRate);
    	void Flush(); // Clear history
        void Reset();

    private:
    	void CalculateCoefficients(uint32_t sampleRate);
    	void ClearDelayLines();

    };

	struct BiquadMirror {
		uint32_t gen = START_GENERATION;

		BiquadParams params;
		bool isParamsDirty = false;

		void Reset() {
			gen++;

			params = BiquadParams{};
			isParamsDirty = false;
		}
	};

}
