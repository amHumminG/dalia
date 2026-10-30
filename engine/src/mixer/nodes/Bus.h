#pragma once

#include "core/Constants.h"
#include "dalia/EffectControl.h"
#include "mixer/dsp/Slew.h"
#include "mixer/nodes/EffectChain.h"

namespace dalia {

	struct BusParams {
		float gain = GAIN_DEFAULT;
	};

    struct Bus {
    	bool isActive = false;

        uint32_t currentParentIndex = NO_PARENT;
    	uint32_t targetParentIndex = NO_PARENT;

        // Mixing Properties
    	float currentFadeGain = GAIN_DEFAULT;
    	float targetFadeGain = GAIN_DEFAULT;

        float targetGain = GAIN_DEFAULT;
        float currentGain = GAIN_DEFAULT;

    	EffectChain effectChain;
    	BusParams params;

    	void SetParams(const BusParams& bParams) { this->params = bParams; }

        void Reset() {
        	isActive = false;

            currentParentIndex = NO_PARENT;
        	targetParentIndex = NO_PARENT;

        	currentFadeGain = GAIN_DEFAULT;
        	targetFadeGain = GAIN_DEFAULT;

    		effectChain.Reset();
    		params = BusParams{};
        }
    };

    struct BusMirror {
        uint32_t refCount = 0;
        uint32_t parentBusIndex = NO_PARENT;

        BusParams params;

        EffectHandle effectSlots[MAX_EFFECTS_PER_BUS] = {
            INVALID_EFFECT_HANDLE,
            INVALID_EFFECT_HANDLE,
            INVALID_EFFECT_HANDLE,
            INVALID_EFFECT_HANDLE,
        };

        void Reset() {
            refCount = 0;
            parentBusIndex = NO_PARENT;

            params = BusParams{};

            for (uint32_t i = 0; i < MAX_EFFECTS_PER_BUS; i++) {
                effectSlots[i] = INVALID_EFFECT_HANDLE;
            }
        }
    };
}
