#pragma once
#include "core/Constants.h"
#include <string>
#include <span>

namespace dalia {

    class Bus {
    public:
        Bus() = default;
        ~Bus() = default;
        void SetName(const std::string& name);
        void SetBuffer(std::span<float> buffer);
        std::span<float> GetBuffer() const;
        void Clear();
        void ApplyDSP(uint32_t sampleCount);
        void MixInBuffer(std::span<float> inBuffer, uint32_t sampleCount);

        uint32_t parentIndex = NO_PARENT;

    private:
        void Reset();

        std::string m_name; // Do we just use this for debug?
        float m_volume = 1.0f;

        std::span<float> m_buffer;
    };

    struct BusMirror {
        bool isBusy = false;
        uint32_t generation = 0;
        uint32_t parentBusIndex = NO_PARENT;

        // Mixing Properties
        float volume = 1.0f;

        void Reset() {
            isBusy = false;
            parentBusIndex = NO_PARENT;
        }
    };
}
