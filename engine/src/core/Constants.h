#pragma once
#include <cstdint>

// Compiler build detection
#if defined(_DEBUG) || !defined(NDEBUG)
    #define DALIA_DEBUG 1
#else
    #define DALIA_DEBUG 0
#endif

// Compiler-specific restrict macros for SIMD auto-vectorization
#if defined(_MSC_VER)
    // Microsoft Visual C++ (Pointer-level restriction)
    #define DALIA_RESTRICT __restrict
#elif defined(__GNUC__) || defined(__clang__)
    // GCC and Clang
    #define DALIA_RESTRICT __restrict__
#else
    // Fallback for unknown compilers
    #define DALIA_RESTRICT
#endif

namespace dalia {

    // -- I/O ---
    static constexpr size_t MAX_IO_PATH_SIZE = 256; // The maximum length of a filepath string

    // --- Mixing ---
    constexpr float VOLUME_EPSILON = 1e-5f;
    constexpr float FREQUENCY_EPSILON = 1e-1f;
    constexpr float RESONANCE_EPSILON = 1e-3f;

    constexpr float SMOOTHING_COEFFICIENT = 0.005f; // Used for volume smoothing (from current to target)
    static constexpr float PI   = 3.14159265358979323846f;
    static constexpr float PI_2 = 1.57079632679489661923f;

    static constexpr float DEFAULT_VOLUME_LINEAR = 1.0f;
    static constexpr float MIN_VOLUME_LINEAR = 0.0f;
    static constexpr float MAX_VOLUME_LINEAR = 15.8489f;

    static constexpr float DEFAULT_VOLUME_DB = 0.0f;
    static constexpr float MIN_VOLUME_DB = -96.0f;  // Minimum volume in decibels
    static constexpr float MAX_VOLUME_DB = 24.0f;   // Maximum volume in decibels

    static constexpr uint32_t CHANNELS_MAX = 8;
    static constexpr uint32_t CHANNELS_MONO = 1;
    static constexpr uint32_t CHANNELS_STEREO = 2;

    static constexpr uint32_t CONTROL_RATE = 32; // Number of frames between effect recalculations

    static constexpr float FILTER_MIN_FREQUENCY = 20.0f;
    static constexpr float FILTER_MAX_FREQUENCY = 20000.0f;
    constexpr float FILTER_MIN_RESONANCE = 0.1f;
    constexpr float FILTER_MAX_RESONANCE = 10.0f;

    static constexpr uint32_t TARGET_OUTPUT_SAMPLE_RATE = 48000; // Only to be used in engine init

    static constexpr size_t MAX_FRAMES_PER_RENDER = 1024; // Maximum frames that we can render per audio callback

    // --- Handles, Indices & Generations ---
    static constexpr uint32_t MASTER_BUS_INDEX = 0; // The index of the master bus within the bus pool
    static constexpr uint32_t NO_PARENT = INT32_MAX; // Indicator that a voice or bus has no parent

    static constexpr uint32_t INVALID_GENERATION = 0;
    static constexpr uint32_t START_GENERATION = 1;

}