#pragma once
#include "dalia/audio/PlaybackControl.h"
#include "dalia/audio/SoundHandle.h"
#include <cstdint>

namespace dalia {

    class Bus;

    enum class VoiceState : uint8_t {
        Free,
        Inactive,
        Playing,
        Virtual,
        Paused,
        Stopped
    };

    struct Voice {
        uint32_t generation = 0;
        VoiceState state = VoiceState::Free;
        PlaybackExitCondition exitCondition = PlaybackExitCondition::NaturalEnd;

        // Routing
        uint32_t parentBusIndex = 0;

        // Mixing Properties
        bool isLooping = false;
        float volume = 1.0f;
        float pitch = 1.0f;
        float pan = 0.0f;

        uint32_t channels = 2;
        uint32_t sampleRate = 48000;
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
            parentBusIndex = 0;
            state = VoiceState::Free;
            exitCondition = PlaybackExitCondition::NaturalEnd;

            isLooping = false;
            volume = 1.0f;
            pitch = 1.0f;
            pan = 0.0f;

            channels = 2;
            sampleRate = 48000;
            cursor = 0.0f;
        }
    };

    struct VoiceMirror {
        // --- API Thread Only Stuff ---
        bool pendingLoad = false; // Pending playback due to asset loading
        AudioEventCallback onStopCallback = nullptr;
        uint64_t assetUuid;

        // --- Voice Properties ---
        uint32_t generation = 0;
        VoiceState state = VoiceState::Free;

        // Routing
        uint32_t parentBusIndex = 0;

        // Mixing Properties
        bool isLooping = false;
        float volume = 1.0f;
        float pitch = 1.0f;
        float pan = 0.0f;

        // Asset
        SoundType soundType;

        void Reset() {
            pendingLoad = false;
            onStopCallback = nullptr;
            assetUuid = 0;

            generation++;
            state = VoiceState::Free;

            parentBusIndex = 0;

            isLooping = false;
            volume = 1.0f;
            pitch = 1.0f;
            pan = 0.0f;
        }
    };
}
