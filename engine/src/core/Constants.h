#pragma once
#include <cstdint>

// Compiler build detection
#if defined(_DEBUG) || !defined(NDEBUG)
    #define DALIA_DEBUG 1
#else
    #define DALIA_DEBUG 0
#endif

namespace dalia {

    // -- I/O ---
    static constexpr size_t MAX_IO_PATH_SIZE = 256; // The maximum length of a filepath string

    // --- Mixing ---
    static constexpr uint32_t CHANNELS_MONO = 1;
    static constexpr uint32_t CHANNELS_STEREO = 2;
    static constexpr uint32_t MASTER_BUS_INDEX = 0; // The index of the master bus within the bus pool
    static constexpr size_t MAX_FRAMES_PER_RENDER = 1024; // Maximum frames that we can render per audio callback
    static constexpr uint32_t NO_PARENT = INT32_MAX; // Indicator that a voice or bus has no parent
}