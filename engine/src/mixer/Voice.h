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
        None,
        Inactive,
        Playing,
        Virtual,
        Paused,
        Stopping
    };

    struct Voice {
        uint32_t generation = 0;

        VoiceState state = VoiceState::None;

        // FIXME: Reference to sound in soundbank (probably unnecessary here)
        uint32_t assetID = 0;

        // Bus routing
        uint32_t parentBusIndex;

        // Playback
        bool isLooping = false;
        float volume = 1.0f;
        float pitch = 1.0f;
        float pan = 0.0f;
        uint32_t channels = 2;  // This is probably read from the soundbank
        float cursor = 0.0f;    // Frame or sample position?

        VoiceSourceType sourceType = VoiceSourceType::None;

        // Resident
        std::span<float> buffer;

        // Streaming
        uint8_t frontBufferIndex = 0;
        uint16_t streamingContextIndex;
        // The StreamingContext should be assigned by the game thread and sent to the audio thread via the play command

        // Use this when releasing a voice
        void Reset() {
            parentBusIndex = 0;
            state = VoiceState::None;
            isLooping = false;
            volume = 1.0f;
            pitch = 1.0f;
            pan = 0.0f;
            frontBufferIndex = 0;
            cursor = 0.0f;
            // TODO: Keep updating this
        }
    };

    struct VoiceMirror {
        uint32_t generation = 0;
        bool isBusy = false;
        uint32_t parentBusIndex;
        VoiceState state = VoiceState::None;
        void* callbackOnFinished = nullptr;
        AudioEventCallback callback = nullptr;
        void* userData = nullptr;
        // We probably keep other voice data here too (volume etc.)

        void Reset() {
            isBusy = false;
            state = VoiceState::None;
            callbackOnFinished = nullptr;
            callback = nullptr;
            userData = nullptr;
        }
    };
}
