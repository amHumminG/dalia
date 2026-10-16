#pragma once

#include "core/Constants.h"
#include "dalia/EffectControl.h"
#include "dsp/Slew.h"

namespace dalia {

    constexpr uint32_t MAX_EFFECTS_PER_BUS = 4;

    enum class EffectState {
        None, // The slot is not used
        FadingIn,
        Active,
        FadingOut
    };

    struct EffectSlot {
        EffectHandle handle = InvalidEffectHandle;
        EffectState state = EffectState::None;
        float currentMix = 0.0f;

        void Reset() {
            handle = InvalidEffectHandle;
            state = EffectState::None;
            currentMix = 0.0f;
        }
    };

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

    	BusParams params;

        EffectSlot effectSlots[MAX_EFFECTS_PER_BUS];

        void Reset() {
        	isActive = false;

            currentParentIndex = NO_PARENT;
        	targetParentIndex = NO_PARENT;

        	currentFadeGain = GAIN_DEFAULT;
        	targetFadeGain = GAIN_DEFAULT;

            for (uint32_t i = 0; i < MAX_EFFECTS_PER_BUS; i++) {
                effectSlots[i].Reset();
            }
        }
    };

    struct BusMirror {
        uint32_t refCount = 0;
        uint32_t parentBusIndex = NO_PARENT;

        BusParams params;

        EffectHandle effectSlots[MAX_EFFECTS_PER_BUS] = {
            InvalidEffectHandle,
            InvalidEffectHandle,
            InvalidEffectHandle,
            InvalidEffectHandle,
        };

        void Reset() {
            refCount = 0;
            parentBusIndex = NO_PARENT;

            params = BusParams{};

            for (uint32_t i = 0; i < MAX_EFFECTS_PER_BUS; i++) {
                effectSlots[i] = InvalidEffectHandle;
            }
        }
    };
}
