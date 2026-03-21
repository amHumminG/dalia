#pragma once
#include "common/StringID.h"

namespace dalia {

    using SoundID = StringID;
    using BusID = StringID;
    // using BankID = StringID;

    // Asset lifecycles
    enum class LoadState : uint8_t {
        Unloaded = 0,
        Loading  = 1,
        Loaded   = 2,
        Error    = 3
    };
}
