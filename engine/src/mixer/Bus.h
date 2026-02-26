#pragma once
#include <string>
#include <vector>
#include <array>
#include <span>

namespace dalia {

    static constexpr size_t MAX_FRAMES_PER_RENDER = 1024;

    class Bus {
    public:
        Bus() = default;
        ~Bus() = default;
        void SetName(const std::string& name);
        void SetBuffer(std::span<float> buffer);
        std::span<float> getBuffer() const;
        void Clear();
        void Pull(std::span<float> outputBuffer, uint32_t sampleCount);
        void AddChild(Bus* child);

    private:
        void ApplyGain();

        std::string m_name; // Do we just use this for debug?
        float m_volume = 1.0f;

        std::span<float> m_buffer;

        Bus* m_children[8];
        size_t m_childCount = 0;
    };
}
