#pragma once

#include "core/Types.h"
#include "dalia/SoundControl.h"

#include <cstdint>
#include <atomic>

namespace dalia {

    struct StreamSound {
        LoadState state = LoadState::Unloaded;
        uint32_t refCount = 0;

        uint32_t pathHash = 0;

        char filepath[MAX_STR_LEN_ASSET_PATH];

        // Will be used if inside a soundbank
        size_t byteOffset = 0;
        size_t byteLenght = 0;

        // Format
        uint32_t totalFrames = 0;
        uint32_t channels = 0;
        uint32_t sampleRate = 0;

    	double lengthInSeconds = 0.0;

        void Reset() {
            state = LoadState::Unloaded;
            refCount = 0;

            pathHash = 0;

            filepath[0] = '\0';
            byteOffset = 0;
            byteLenght = 0;

            totalFrames = 0;
            channels = 0;
            sampleRate = 0;
        }
    };
}