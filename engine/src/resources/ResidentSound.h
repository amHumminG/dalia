#pragma once

#include "core/Types.h"
#include <vector>

#include <span>
#include <atomic>

namespace dalia {

    struct ResidentSound {
        std::atomic<LoadState> state{LoadState::Unloaded};
        uint32_t refCount = 0;

        uint32_t pathHash = 0;

        std::vector<float> pcmData;
        uint32_t channels = 0;
        uint32_t sampleRate = 0;
        uint32_t frameCount = 0; // Do we even care about this? probably not

        std::span<float> GetBuffer() {
            return std::span<float>(pcmData.data(), pcmData.size());
        }

        void Reset() {
            state.store(LoadState::Unloaded, std::memory_order_relaxed);
            refCount = 0;

            pathHash = 0;

            pcmData.clear();
            channels = 0;
            sampleRate = 0;
            frameCount = 0;
        }
    };
}