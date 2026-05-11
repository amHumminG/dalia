#pragma once
#include "core/Constants.h"
#include "dalia/audio/EffectControl.h"

namespace dalia {

    constexpr uint32_t MAX_EFFECTS_PER_BUS = 4;

    enum class EffectState {
        None, // The slot is not used
        FadingIn,
        Active,
        FadingOut
    };

    struct EffectSlot {
        EffectHandle effect = InvalidEffectHandle;
        EffectState state = EffectState::None;
        float currentMix = 0.0f;

        void Reset() {
            effect = InvalidEffectHandle;
            state = EffectState::None;
            currentMix = 0.0f;
        }
    };

    struct Bus {
    	bool isActive = false;

        uint32_t currentParentIndex = NO_PARENT;
    	uint32_t targetParentIndex = NO_PARENT;

        // Mixing Properties
    	float currentFadeGain = 1.0f;
    	float targetFadeGain = 1.0f;

        float targetGain = GAIN_DEFAULT;
        float currentGain = GAIN_DEFAULT;

        EffectSlot effectSlots[MAX_EFFECTS_PER_BUS];

        void Reset() {
        	isActive = false;

            currentParentIndex = NO_PARENT;
        	targetParentIndex = NO_PARENT;

        	currentFadeGain = 1.0f;
        	targetFadeGain = 1.0f;

            targetGain = GAIN_DEFAULT;
            currentGain = GAIN_DEFAULT;

            for (int i = 0; i < MAX_EFFECTS_PER_BUS; i++) {
                effectSlots[i].Reset();
            }
        }
    };

    struct BusMirror {
        uint32_t refCount = 0;
        uint32_t parentBusIndex = NO_PARENT;

        // Mixing Properties
        float volumeDb = VOLUME_DB_DEFAULT;
        EffectHandle effectSlots[MAX_EFFECTS_PER_BUS] = {
            InvalidEffectHandle,
            InvalidEffectHandle,
            InvalidEffectHandle,
            InvalidEffectHandle,
        };

        void Reset() {
            refCount = 0;
            parentBusIndex = NO_PARENT;

            volumeDb = VOLUME_DB_DEFAULT;
            for (int i = 0; i < MAX_EFFECTS_PER_BUS; i++) {
                effectSlots[i] = InvalidEffectHandle;
            }
        }
    };
}
