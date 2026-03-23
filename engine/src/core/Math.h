#pragma once
#include "core/Constants.h"

#include <cmath>

namespace dalia {

    inline float DbToLinear(float db) {
        if (db <= MIN_VOLUME_DB) return MIN_VOLUME_LINEAR;
        if (db >= MAX_VOLUME_DB) return MAX_VOLUME_LINEAR;

        return powf(10.0f, db / 20.0f);
    }

    inline float LinearToDb(float linear) {
        if (linear <= MIN_VOLUME_LINEAR) return MIN_VOLUME_DB;
        if (linear >= MAX_VOLUME_LINEAR) return MAX_VOLUME_DB;

        return 20.0f * log10f(linear);
    }
}