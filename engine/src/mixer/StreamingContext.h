#pragma once
#include <cstdint>
#include <array>
#include <atomic>
#include <algorithm>

namespace dalia {

    static constexpr size_t DOUBLE_BUFFER_CHUNK_SIZE = 16384;
    static constexpr uint32_t NO_EOF = UINT32_MAX;

    struct StreamingContext {
        std::atomic<uint32_t> generation{0};

        uint8_t frontBufferIndex = 0;
        // Should chunk size be multiplied with number of output channels?
        alignas(64) float buffers[2][DOUBLE_BUFFER_CHUNK_SIZE];
        std::array<std::atomic<bool>, 2> bufferReady{false, false};
        std::array<uint32_t, 2> eofIndex = {NO_EOF, NO_EOF};

        // File Data (sound data in soundbank)
        void* fileHandle = nullptr;     // Subject to change
        size_t fileSize = 0;
        size_t currentFileOffset = 0;   // File offset in soundbank
        size_t readOffset = 0;          // Read offset from file start

        // Use after the StreamingContext is released by a Voice
        void Reset() {
            frontBufferIndex = 0;
            std::fill_n(&buffers[0][0], (2 * DOUBLE_BUFFER_CHUNK_SIZE), 0.0f);
            bufferReady[0].store(false);
            bufferReady[1].store(false);
            eofIndex = {NO_EOF, NO_EOF};

            fileHandle = nullptr;
            fileSize = 0;
            currentFileOffset = 0;
            readOffset = 0;
        }
    };
}
