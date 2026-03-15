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
        Inactive,
        Playing,
        Virtual,
        Paused,

        Finished,
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
        uint32_t generation = 0;
        bool isBusy = false;
        uint32_t parentBusIndex;
        VoiceState state = VoiceState::Free;
        void* callbackOnFinished = nullptr;
        AudioEventCallback callback = nullptr;
        void* userData = nullptr;
        // We probably keep other voice data here too (volume etc.)

        void Reset() {
            isBusy = false;
            state = VoiceState::Free;
            callbackOnFinished = nullptr;
            callback = nullptr;
            userData = nullptr;
        }
    };
}
