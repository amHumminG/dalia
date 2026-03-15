#pragma once
#include <algorithm>

#include "core/States.h"
#include "core/Constants.h"
#include <cstdint>
#include <atomic>

namespace dalia {

    struct StreamSound {
        std::atomic<LoadState> state{LoadState::Unloaded};
        uint32_t refCount = 0;

        uint32_t pathHash = 0;

        char filepath[MAX_IO_PATH_SIZE];

        // Used if inside a soundbank
        size_t byteOffset = 0;
        size_t byteLenght = 0;

        // Format
        uint32_t channels = 0;
        uint32_t sampleRate = 0;
        uint32_t totalFrames = 0;

        void Reset() {
            state.store(LoadState::Unloaded, std::memory_order_relaxed);
            refCount = 0;

            pathHash = 0;

            filepath[0] = '\0';
            byteOffset = 0;
            byteLenght = 0;

            channels = 0;
            sampleRate = 0;
            totalFrames = 0;
        }
    };
}