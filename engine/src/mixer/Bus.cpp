#pragma once
#include "Bus.h"

#include <algorithm>

namespace dalia {

    void Bus::Reset() {
        m_parentBusIndex = NO_PARENT;

        m_volumeLinear = DEFAULT_VOLUME_LINEAR;
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
        if (m_volumeLinear == DEFAULT_VOLUME_LINEAR) return;

        if (m_volumeLinear <= MIN_VOLUME_LINEAR) {
            std::ranges::fill(m_buffer.subspan(0, sampleCount), MIN_VOLUME_LINEAR);
            return;
        }

        for (uint32_t i = 0; i < sampleCount; i++) {
            m_buffer[i] *= m_volumeLinear;
        }

        // This is where we apply effects and filters in the future!
    }

    void Bus::MixInBuffer(std::span<float> inBuffer, uint32_t sampleCount) {
        for (uint32_t i = 0; i < sampleCount; i++) {
            m_buffer[i] += inBuffer[i];
        }
    }
}
