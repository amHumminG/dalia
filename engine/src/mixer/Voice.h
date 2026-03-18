#pragma once
#include "dalia/audio/PlaybackControl.h"
#include "dalia/audio/SoundHandle.h"
#include <cstdint>
#include <span>

namespace dalia {

    class Bus;

    enum class VoiceState : uint8_t {
        Free,
        Inactive,
        Playing,
        Virtual,
        Paused,

        Finished,
        Stopped,
        Killed
    };

    struct Voice {
        uint32_t generation = 0;

        VoiceState state = VoiceState::Free;

        // Bus routing
        uint32_t parentBusIndex;

        // Playback
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
                uint32_t frames;
            } resident;

            struct {
                uint32_t streamContextIndex;
                uint8_t frontBufferIndex;
            } stream;
        } data;

        // Use this when releasing a voice
        void Reset() {
            parentBusIndex = 0;
            isLooping = false;
            volume = 1.0f;
            pitch = 1.0f;
            pan = 0.0f;
            cursor = 0.0f;
            // TODO: Keep updating this
        }
    };

    struct VoiceMirror {
        // TODO: Expand this to hold all voice parameters
        bool pendingLoad = false; // Used by the api thread only

        VoiceState state = VoiceState::Free;
        uint32_t generation = 0;

        // Asset
        SoundType soundType;
        uint64_t assetUuid;

        // Parameters
        uint32_t parentBusIndex;

        // Callback
        AudioEventCallback onFinishedCallback = nullptr;
        void* userData = nullptr;

        void Reset() {
            pendingLoad = false;

            state = VoiceState::Free;
            generation++;

            assetUuid = 0;

            onFinishedCallback = nullptr;
            userData = nullptr;
        }
    };
}
