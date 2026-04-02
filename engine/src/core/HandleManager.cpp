#include "core/HandleManager.h"

#include "core/Constants.h"

namespace dalia {

    HandleManager::HandleManager(uint32_t capacity)
        : m_capacity(capacity),
        m_isUsed(std::make_unique<bool[]>(capacity)),
        m_generations(std::make_unique<uint32_t[]>(capacity)),
        m_freeIndices(capacity) {
        for (uint32_t i = capacity; i-- > 0;) {
            m_isUsed[i] = false;
            m_generations[i] = START_GENERATION;
            m_freeIndices.Push(i);
        }
    }

    bool HandleManager::Allocate(uint32_t& outIndex, uint32_t& outGeneration) {
        uint32_t index;
        if (!m_freeIndices.Pop(index)) return false;
        m_isUsed[index] = true;

        outIndex = index;
        outGeneration = m_generations[index];

        return true;
    }

    bool HandleManager::Free(uint32_t index, uint32_t generation) {
        if (!IsValid(index, generation)) return false;

        m_generations[index]++;
        if (m_generations[index] == NO_GENERATION) m_generations[index] = START_GENERATION;
        m_freeIndices.Push(index);
        m_isUsed[index] = false;

        return true;
    }

    uint32_t HandleManager::GetCapacity() const {
        return m_capacity;
    }

    bool HandleManager::IsValid(uint32_t index, uint32_t generation) const {
        if (index >= m_capacity) return false;
        if (!m_isUsed[index]) return false;
        if (m_generations[index] != generation) return false;

        return true;
    }
}
