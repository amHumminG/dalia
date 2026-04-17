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
	constexpr float EPSILON = 1e-5f;
    constexpr float EPSILON_VOLUME = 1e-5f;
    constexpr float EPSILON_GAIN = 1e-5f;
    constexpr float EPSILON_PAN = 1e-5f;
    constexpr float EPSILON_FREQUENCY = 1e-1f;
    constexpr float EPSILON_RESONANCE = 1e-3f;

    constexpr float FADE_TIME_GAIN = 0.02f;			// Time (in seconds) to fade gain from 0.0 to 1.0
    constexpr float SMOOTHING_CUTOFF_HZ = 15.0f;    // Determines the smoothing time for some gain changes
    static constexpr double dPI = 3.14159265358979323846f;
    static constexpr float PI	= 3.14159265358979323846f;
    static constexpr float PI_2 = 1.57079632679489661923f;

    static constexpr float GAIN_DEFAULT = 1.0f;
	static constexpr float GAIN_MIN = 0.0f;
	static constexpr float GAIN_MAX = 15.8489f;
	static constexpr float GAIN_SILENCE = GAIN_MIN;

    static constexpr float VOLUME_DB_DEFAULT = 0.0f;
    static constexpr float VOLUME_DB_MIN = -96.0f;  // Minimum volume in decibels
    static constexpr float VOLUME_DB_MAX = 24.0f;   // Maximum volume in decibels

	static constexpr float PITCH_DEFAULT = 1.0f;

	// Panning
	static constexpr float PAN_STEREO_DEFAULT = 0.0f;
    static constexpr float PAN_STEREO_MIN = -1.0f;
    static constexpr float PAN_STEREO_MAX = 1.0f;
    static constexpr float PAN_STEREO_LEFT = PAN_STEREO_MIN;
	static constexpr float PAN_STEREO_MIDDLE = PAN_STEREO_DEFAULT;
    static constexpr float PAN_STEREO_RIGHT = PAN_STEREO_MAX;

	// Channels
	static constexpr uint32_t CHANNELS_MONO = 1;
	static constexpr uint32_t CHANNELS_STEREO = 2;
    static constexpr uint32_t CHANNELS_MAX = 8;

	// Listeners
	static constexpr uint32_t LISTENERS_DEFAULT = 1;
	static constexpr uint32_t LISTENERS_MIN		= 1;
	static constexpr uint32_t LISTENERS_MAX		= 4;

	// Distance attenuation
	static constexpr float MIN_DIST_DEFAULT = 1.0f;
	static constexpr float MAX_DIST_DEFAULT = 100.0f;
	static constexpr float MIN_DIST_MIN = 0.01f;
	static constexpr float MAX_DIST_MIN = 0.01f;


    static constexpr uint32_t CONTROL_RATE = 32; // Number of frames between effect recalculations

    static constexpr float FILTER_FREQUENCY_MIN = 20.0f;
    static constexpr float FILTER_FREQUENCY_MAX = 20000.0f;
    constexpr float FILTER_RESONANCE_MIN = 0.1f;
    constexpr float FILTER_RESONANCE_MAX = 10.0f;

    static constexpr uint32_t TARGET_OUTPUT_SAMPLE_RATE = 48000; // The output sample rate the engine wants to output

    // --- Handles, Indices & Generations ---
    static constexpr uint64_t INVALID_RAW_ID = 0;

    static constexpr uint32_t MASTER_BUS_INDEX = 0;  // The index of the master bus within the bus pool
    static constexpr uint32_t NO_PARENT = INT32_MAX; // Indicator that a voice or bus has no parent
    static constexpr uint32_t NO_INDEX = INT32_MAX;  // Default index value

    static constexpr uint32_t NO_GENERATION = 0;
    static constexpr uint32_t START_GENERATION = 1;

}