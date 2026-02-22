#pragma once
#include "StreamingContext.h"
#include <cstdint>
#include <span>

namespace dalia {

    enum class VoiceSourceType {
        Resident,   // Played from RAM
        Stream      // Streamed from soundbank (double buffered)
    };

    struct Voice {
        uint32_t assetID = 0; // Subject to change depending on how sounds are identified in banks
        uint64_t uuid; // AudioHandle uuid

        // Playback
        bool isPlaying = false;
        bool isLooping = false;
        float volume = 1.0f;
        float pitch = 1.0f;
        float pan = 0.0f;
        uint32_t channels = 2; // This is probably read from the soundbank
        float cursor = 0.0f; // Frame or sample position

        VoiceSourceType sourceType; // Default to resident?
        std::span<float> buffer;
        StreamingContext* streamingContext; // This could also be an index to the StreamingContext pool
        // The StreamingContext should be assigned by the game thread and sent to the audio thread via the play command

        // To be used before a new sound is played by voice
        void Reset() {

        }
    };

    struct VoiceSlot {
        uint32_t generation = 0;
        bool isBusy = false;
    };
}
