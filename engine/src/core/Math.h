#pragma once
#include "core/Constants.h"

#include <cmath>

namespace dalia {

    inline float DbToGain(float db) {
        if (db <= VOLUME_DB_MIN) return GAIN_MIN;
        if (db >= VOLUME_DB_MAX) return GAIN_MAX;

        return powf(10.0f, db / 20.0f);
    }

    inline float GainToDb(float linear) {
        if (linear <= GAIN_MIN) return VOLUME_DB_MIN;
        if (linear >= GAIN_MAX) return VOLUME_DB_MAX;

        return 20.0f * log10f(linear);
    }

    inline bool NearlyEqual(float a, float b, float epsilon) {
        return std::abs(a - b) < epsilon;
    }
}