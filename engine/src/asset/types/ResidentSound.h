#pragma once

#include "core/Types.h"
#include <vector>

#include <span>
#include <atomic>

namespace dalia {

    struct ResidentSound {
        LoadState state = LoadState::Unloaded;
        uint32_t refCount = 0;

        uint32_t pathHash = 0;

        std::vector<float> pcmData;

        // Format
        uint32_t totalFrames = 0;
        uint32_t channels = 0;
        uint32_t sampleRate = 0;

    	double lengthInSeconds = 0.0;

        std::span<float> GetBuffer() { return std::span(pcmData.data(), pcmData.size()); }

        void Reset() {
            state = LoadState::Unloaded;
            refCount = 0;

            pathHash = 0;

            pcmData.clear();
            totalFrames = 0;
            channels = 0;
            sampleRate = 0;
        }
    };
}