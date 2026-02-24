#pragma once
#include <cstdint>
#include <array>
#include <atomic>

namespace dalia {

    static constexpr size_t DOUBLE_BUFFER_CHUNK_SIZE = 16384;

    struct StreamingContext {
        bool inUse = false; // Might not be needed?

        alignas(64) float buffers[2][DOUBLE_BUFFER_CHUNK_SIZE];
        uint8_t frontBufferIndex = 0;

        std::array<bool, 2> bufferReady = {false, false};
        bool isWaitingForIO = false;

        size_t currentFileOffset = 0; // Probably needed

        void* fileHandle = nullptr; // Subject to change
        size_t readOffset = 0;

        // Use after the StreamingContext is released by a Voice
        void Reset() {
            frontBufferIndex = 0;
            bufferReady = {false, false};
            std::memset(buffers, 0, sizeof(buffers));
            isWaitingForIO = false;
        }
    };
}
