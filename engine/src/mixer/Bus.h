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
        uint32_t parentBusIndex = NO_PARENT;

        // Mixing Properties
        float targetVolumeLinear = DEFAULT_VOLUME_LINEAR; // Stored as linear amplitude
        float currentVolumeLinear = DEFAULT_VOLUME_LINEAR;
        EffectSlot effectSlots[MAX_EFFECTS_PER_BUS];

        void Reset() {
            parentBusIndex = NO_PARENT;

            targetVolumeLinear = DEFAULT_VOLUME_LINEAR;
            currentVolumeLinear = DEFAULT_VOLUME_LINEAR;
            for (int i = 0; i < MAX_EFFECTS_PER_BUS; i++) {
                effectSlots[i].Reset();
            }
        }
    };

    struct BusMirror {
        uint32_t refCount = 0;
        uint32_t parentBusIndex = NO_PARENT;

        // Mixing Properties
        float volumeDb = DEFAULT_VOLUME_DB; // Stored in decibels
        EffectHandle effectSlots[MAX_EFFECTS_PER_BUS] = {
            InvalidEffectHandle,
            InvalidEffectHandle,
            InvalidEffectHandle,
            InvalidEffectHandle,
        };

        void Reset() {
            refCount = 0;
            parentBusIndex = NO_PARENT;

            volumeDb = DEFAULT_VOLUME_DB;
            for (int i = 0; i < MAX_EFFECTS_PER_BUS; i++) {
                effectSlots[i] = InvalidEffectHandle;
            }
        }
    };
}
