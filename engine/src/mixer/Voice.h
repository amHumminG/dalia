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

    struct Voice {
        uint32_t generation = 0;

        // FIXME: Reference to sound in soundbank (probably unnecessary here)
        uint32_t assetID = 0;

        // Bus routing
        uint32_t parentBusIndex;

        // Playback
        bool isPlaying = false;
        bool isLooping = false;
        float volume = 1.0f;
        float pitch = 1.0f;
        float pan = 0.0f;
        uint32_t channels = 2;  // This is probably read from the soundbank
        float cursor = 0.0f;    // Frame or sample position?

        VoiceSourceType sourceType = VoiceSourceType::None;
        std::span<float> buffer;
        uint16_t streamingContextIndex;
        // The StreamingContext should be assigned by the game thread and sent to the audio thread via the play command

        // To be used before a new sound is played by voice
        void Reset() {
            parentBusIndex = 0;
            isPlaying = false;
            isLooping = false;
            volume = 1.0f;
            pitch = 1.0f;
            pan = 0.0f;
            cursor = 0.0f;
        }
    };

    struct VoiceMirror {
        bool isBusy = false;
        uint32_t generation = 0;
        void* callbackOnFinished = nullptr;
        AudioEventCallback callback = nullptr;
        void* userData = nullptr;

        // We probably keep other voice data here too (volume etc.)
    };
}
