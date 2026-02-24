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

    std::span<float> Bus::getBuffer() const {
        return m_buffer;
    }

    void Bus::Clear() {
        std::ranges::fill(m_buffer, 0.0f);
    }

    void Bus::Pull(std::span<float> outputBuffer, uint32_t sampleCount) {
        for (size_t i = 0; i < m_childCount; i++) {
            m_children[i]->Pull(m_buffer, sampleCount);
        }

        // Apply effects here

        for (size_t i = 0; i < sampleCount; i++) {
            outputBuffer[i] += m_buffer[i];
        }
    }
}
