#pragma once
#include "dalia/audio/PlaybackControl.h"
#include "dalia/audio/SoundControl.h"
#include "core/Constants.h"
#include <cstdint>

#include "StreamContext.h"

namespace dalia {

    enum class VoiceState : uint8_t {
        Free,
        Inactive,
        Playing,
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
        float targetGainMatrix[CHANNELS_MAX][CHANNELS_MAX];
        float currentGainMatrix[CHANNELS_MAX][CHANNELS_MAX];
        float pitch = 1.0f;

        uint32_t channels = CHANNELS_STEREO;
        uint32_t sampleRate = TARGET_OUTPUT_SAMPLE_RATE;
        double cursor = 0.0f;

        SoundType soundType = SoundType::None;
        union {
            struct {
                const float* pcmData;
                uint32_t frameCount;
            } resident;

            struct {
                uint32_t streamContextIndex;
                uint32_t frontBufferIndex;
            } stream;
        } data = {};

        // Use this when releasing a voice
        void Reset() {
            gen++;
            if (gen == NO_GENERATION) gen = START_GENERATION;

            parentBusIndex = MASTER_BUS_INDEX;
            state = VoiceState::Free;
            exitCondition = PlaybackExitCondition::NaturalEnd;

            isLooping = false;
            std::memset(targetGainMatrix, 0.0f, sizeof(targetGainMatrix));
            std::memset(currentGainMatrix, 0.0f, sizeof(currentGainMatrix));

            channels = 0;
            sampleRate = 0;
            cursor = 0.0f;

            soundType = SoundType::None;
            data = {};
        }
    };

    struct VoiceMirror {
        // --- API Thread Only Stuff ---
        bool pendingLoad = false; // Pending playback due to asset loading
        AudioEventCallback onStopCallback = nullptr;
        uint64_t assetUuid;

        bool isGainDirty = true;
        float volumeDb = DEFAULT_VOLUME_DB;
        float stereoPan = 0.0f;

        bool isSpatial = false;
        // Vector3 position
        // Vector3 velocity

        // --- Voice Properties ---
        uint32_t gen = START_GENERATION;
        VoiceState state = VoiceState::Free;

        // Routing
        uint32_t parentBusIndex = MASTER_BUS_INDEX;

        // Mixing Properties
        bool isLooping = false;
        float pitch = 1.0f;

        // Asset
        uint32_t channels = 0;
        SoundType soundType;

        void Reset() {
            pendingLoad = false;
            onStopCallback = nullptr;
            assetUuid = 0;

            isGainDirty = true;
            volumeDb = DEFAULT_VOLUME_DB;
            stereoPan = 0.0f;

            gen++;
            if (gen == NO_GENERATION) gen = START_GENERATION;
            state = VoiceState::Free;

            parentBusIndex = MASTER_BUS_INDEX;

            isLooping = false;
            pitch = 1.0f;

            channels = 0;
            soundType = SoundType::None;
        }
    };
}
