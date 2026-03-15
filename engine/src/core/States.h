#pragma once

namespace dalia {

    // Asset lifecycles
    enum class LoadState : uint8_t {
        Unloaded = 0,
        Loading  = 1,
        Loaded   = 2,
        Error    = 3
    };
}