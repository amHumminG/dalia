#pragma once
#include "core/FixedStack.h"
#include "core/Constants.h"
#include <memory>
#include <cstdint>

namespace dalia {

    template <typename T>
    class ResourcePool {
    public:
        ResourcePool(uint32_t capacity)
            : m_capacity(capacity),
            m_slots(std::make_unique<Slot[]>(capacity)),
            m_freeSlotsIndices(capacity) {

            for (int i = static_cast<int>(capacity) - 1; i >= 0; i--) {
                m_freeSlotsIndices.Push(static_cast<uint32_t>(i));
            }
        }

        ResourcePool(const ResourcePool&) = delete;
        ResourcePool& operator=(const ResourcePool&) = delete;

        bool Allocate(uint32_t& outIndex, uint32_t& outGeneration) {
            uint32_t slotIndex;
            if (!m_freeSlotsIndices.Pop(slotIndex)) return false;

            outIndex = slotIndex;
            outGeneration = m_slots[slotIndex].generation;

            return true;
        }

        void Free(uint32_t index, uint32_t generation) {
            if (index >= m_capacity) return;
            if (m_slots[index].generation == generation) {
                m_slots[index].generation++;
                if (m_slots[index].generation == INVALID_GENERATION) m_slots[index].generation = START_GENERATION;
                m_freeSlotsIndices.Push(index);
            }
        }

        T* Get(uint32_t index, uint32_t generation) {
            if (index >= m_capacity) return nullptr;
            if (m_slots[index].generation != generation) return nullptr;

            return &m_slots[index].resource;
        }

    private:
        struct Slot {
            T resource;
            uint32_t generation = START_GENERATION;
        };

        uint32_t m_capacity;
        std::unique_ptr<Slot[]> m_slots;
        FixedStack<uint32_t> m_freeSlotsIndices;
    };
}
