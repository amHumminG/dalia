#pragma once
#include <string>
#include <vector>
#include <array>
#include <span>

namespace dalia {

    static constexpr size_t MAX_FRAMES_PER_RENDER = 1024;
    static constexpr uint32_t NO_PARENT = INT32_MAX;

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

        uint32_t parentIndex;

    private:
        std::string m_name; // Do we just use this for debug?
        float m_volume = 1.0f;

        std::span<float> m_buffer;
    };

    struct BusMirror {
        bool isBusy = false;
        uint32_t generation = 0;
        uint32_t parentBusIndex = NO_PARENT;

        // We probably keep other bus data here too (volume etc.)
    };
}
