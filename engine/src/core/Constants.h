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
    static constexpr float DEFAULT_VOLUME_LINEAR = 1.0f;
    static constexpr float MIN_VOLUME_LINEAR = 0.0f;
    static constexpr float MAX_VOLUME_LINEAR = 15.8489f;

    static constexpr float DEFAULT_VOLUME_DB = 0.0f;
    static constexpr float MIN_VOLUME_DB = -96.0f;  // Minimum volume in decibels
    static constexpr float MAX_VOLUME_DB = 24.0f;   // Maximum volume in decibels

    static constexpr uint32_t CHANNELS_MONO = 1;
    static constexpr uint32_t CHANNELS_STEREO = 2;

    static constexpr uint32_t OUTPUT_SAMPLE_RATE = 48000;

    static constexpr size_t MAX_FRAMES_PER_RENDER = 1024; // Maximum frames that we can render per audio callback

    // --- Handles, Indices & Generations ---
    static constexpr uint32_t MASTER_BUS_INDEX = 0; // The index of the master bus within the bus pool
    static constexpr uint32_t NO_PARENT = INT32_MAX; // Indicator that a voice or bus has no parent

    static constexpr uint32_t INVALID_GENERATION = 0;
    static constexpr uint32_t START_GENERATION = 1;

}