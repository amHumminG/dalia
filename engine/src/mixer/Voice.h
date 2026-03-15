#pragma once
#include "dalia/audio/PlaybackControl.h"
#include <cstdint>
#include <span>

namespace dalia {

    class Bus;

    enum class VoiceSourceType {
        None,
        Resident,   // Played from RAM
        Stream      // Streamed from soundbank (double buffered)
    };

    enum class VoiceState : uint8_t {
        Free,
        PendingLoadInactive, // Goes from Pending -> Inactive on load event
        PendingLoadPlaying,  // Goes from Pending -> Playing on load event
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

        VoiceSourceType sourceType = VoiceSourceType::None;
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
        uint32_t generation = 0;
        bool isBusy = false;
        VoiceState state = VoiceState::Free;

        // Asset
        VoiceSourceType sourceType = VoiceSourceType::None;
        uint64_t assetUuid;

        // Parameters
        uint32_t parentBusIndex;

        // Callback
        AudioEventCallback onFinishedCallback = nullptr;
        void* userData = nullptr;

        void Reset() {
            isBusy = false;
            state = VoiceState::Free;
            onFinishedCallback = nullptr;
            userData = nullptr;
        }
    };
}
