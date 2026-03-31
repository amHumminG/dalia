#pragma once
#include "dalia/audio/PlaybackControl.h"
#include "dalia/audio/SoundControl.h"
#include "core/Constants.h"
#include <cstdint>

namespace dalia {

    enum class VoiceState : uint8_t {
        Free,
        Inactive,
        Playing,
        Virtual, // Should probably not be a state
        Paused,
        Stopped
    };

    struct Voice {
        uint32_t gen = START_GENERATION;
        VoiceState state = VoiceState::Free;
        PlaybackExitCondition exitCondition = PlaybackExitCondition::NaturalEnd;

        // Routing
        uint32_t parentBusIndex = MASTER_BUS_INDEX;

        // Mixing Properties
        bool isLooping = false;
        float volumeLinear = DEFAULT_VOLUME_LINEAR;
        float pitch = 1.0f;
        float pan = 0.0f;

        uint32_t channels = CHANNELS_STEREO;
        uint32_t sampleRate = TARGET_OUTPUT_SAMPLE_RATE;
        double cursor = 0.0f;

        SoundType soundType;
        union {
            struct {
                const float* pcmData;
                uint32_t frameCount;
            } resident;

            struct {
                uint32_t streamContextIndex;
                uint8_t frontBufferIndex;
            } stream;
        } data = {};

        // Use this when releasing a voice
        void Reset() {
            gen++;
            if (gen == INVALID_GENERATION) gen = START_GENERATION;

            parentBusIndex = MASTER_BUS_INDEX;
            state = VoiceState::Free;
            exitCondition = PlaybackExitCondition::NaturalEnd;

            isLooping = false;
            volumeLinear = DEFAULT_VOLUME_LINEAR;
            pitch = 1.0f;
            pan = 0.0f;

            channels = CHANNELS_STEREO;
            sampleRate = TARGET_OUTPUT_SAMPLE_RATE;
            cursor = 0.0f;
        }
    };

    struct VoiceMirror {
        // --- API Thread Only Stuff ---
        bool pendingLoad = false; // Pending playback due to asset loading
        AudioEventCallback onStopCallback = nullptr;
        uint64_t assetUuid;

        // --- Voice Properties ---
        uint32_t gen = START_GENERATION;
        VoiceState state = VoiceState::Free;

        // Routing
        uint32_t parentBusIndex = MASTER_BUS_INDEX;

        // Mixing Properties
        bool isLooping = false;
        float volumeDb = DEFAULT_VOLUME_DB;
        float pitch = 1.0f;
        float pan = 0.0f;

        // Asset
        SoundType soundType;

        void Reset() {
            pendingLoad = false;
            onStopCallback = nullptr;
            assetUuid = 0;

            gen++;
            if (gen == INVALID_GENERATION) gen = START_GENERATION;
            state = VoiceState::Free;

            parentBusIndex = MASTER_BUS_INDEX;

            isLooping = false;
            volumeDb = DEFAULT_VOLUME_DB;
            pitch = 1.0f;
            pan = 0.0f;
        }
    };
}
