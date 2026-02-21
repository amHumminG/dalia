#pragma once
#include <cstdint>
#include <atomic>

namespace dalia {

    static constexpr size_t CHUNK_SIZE = 16384;

    struct StreamingContext {
        bool inUse = false; // Might not be needed?

        float buffers[2][CHUNK_SIZE];
        uint8_t frontBufferIndex = 0;

        bool isBufferReady[2] = {false, false};
        bool isWaitingForIO = false;

        uint32_t assetID = 0; // Subject to change (should this be here or in Voice?)
        size_t currentFileOffset = 0; // Probably needed

        void* fileHandle = nullptr; // Subject to change
        size_t readOffset = 0;

        void Reset(); // Use after the StreamingContext is released by a Voice
    };
}
