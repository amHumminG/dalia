#pragma once
#include "Bus.h"

#include <algorithm>

namespace dalia {
    void Bus::SetName(const std::string& name) {
        m_name = name;
    }

    void Bus::SetBuffer(std::span<float> buffer) {
        m_buffer = buffer;
    }

    std::span<float> Bus::GetBuffer() const {
        return m_buffer;
    }

    void Bus::Clear() {
        std::ranges::fill(m_buffer, 0.0f);
    }

    void Bus::ApplyDSP(uint32_t sampleCount) {
        if (m_volume == 1.0f) return;

        if (m_volume <= 0.0f) {
            std::ranges::fill(m_buffer.subspan(0, sampleCount), 0.0f);
            return;
        }

        for (uint32_t i = 0; i < sampleCount; i++) {
            m_buffer[i] *= m_volume;
        }

        // This is where we apply effects and filters in the future!
    }

    void Bus::MixInBuffer(std::span<float> inBuffer, uint32_t sampleCount) {
        for (uint32_t i = 0; i < sampleCount; i++) {
            m_buffer[i] += inBuffer[i];
        }
    }
}
