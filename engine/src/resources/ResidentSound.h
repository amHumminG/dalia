#pragma once
#include <vector>
#include <span>
#include <atomic>

namespace dalia {

    enum class LoadState {
        Unloaded,
        Loading,
        Loaded,
        Error
    };

    struct ResidentSound {
        std::atomic<LoadState> state{LoadState::Unloaded};

        std::vector<float> data;
        uint32_t channels = 0;
        uint32_t sampleRate = 0;
        uint32_t totalFrames = 0;

        std::span<float> GetBuffer() {
            return std::span<float>(data.data(), data.size());
        }
    };
}