#pragma once
#include <cstdint>

namespace dalia {

    // -- I/O ---
    static constexpr size_t MAX_IO_PATH_SIZE = 256; // The maximum length of a filepath string

    // --- Mixing ---
    static constexpr uint8_t CHANNELS_STEREO = 2;
    static constexpr uint32_t MASTER_BUS_INDEX = 0; // The index of the master bus within the bus pool
}