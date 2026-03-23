#pragma once
#include "core/Constants.h"
#include <string>
#include <span>

namespace dalia {

    class Bus {
    public:
        Bus() = default;
        ~Bus() = default;

        void Reset();

        void SetBuffer(std::span<float> buffer);
        std::span<float> GetBuffer() const;
        void Clear();
        void ApplyDSP(uint32_t sampleCount);
        void MixInBuffer(std::span<float> inBuffer, uint32_t sampleCount);

        uint32_t m_parentBusIndex = NO_PARENT;

        // Mixing Properties
        float m_volumeLinear = DEFAULT_VOLUME_LINEAR; // Stored as linear amplitude

    private:
        std::span<float> m_buffer;

    };

    struct BusMirror {
        uint32_t refCount = 0;
        uint32_t parentBusIndex = NO_PARENT;

        // Mixing Properties
        float volumeDb = DEFAULT_VOLUME_DB; // Stored in decibels

        void Reset() {
            refCount = 0;
            parentBusIndex = NO_PARENT;

            volumeDb = DEFAULT_VOLUME_DB;
        }
    };
}
