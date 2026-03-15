#pragma once
#include "core/FixedStack.h"
#include "core/Logger.h"
#include <memory>
#include <cstdint>

namespace dalia {

    template <typename ResourceType, typename HandleType>
    class HandlePool {
    public:
        HandlePool(uint32_t capacity)
            : m_capacity(capacity),
            m_slots(std::make_unique<Slot[]>(capacity)),
            m_freeSlotsIndices(capacity) {

            for (int i = capacity - 1; i >= 0; i--) {
                m_freeSlotsIndices.Push(static_cast<uint32_t>(i));
            }
        }

        HandlePool(const HandlePool&) = delete;
        HandlePool& operator=(const HandlePool&) = delete;

        HandleType Allocate() {
            uint32_t slotIndex;

            if (!m_freeSlotsIndices.Pop(slotIndex)) {
                return HandleType{}; // Invalid handle
            }

            return HandleType::Create(slotIndex, m_slots[slotIndex].generation);
        }

        void Free(HandleType handle) {
            if (!handle.IsValid()) return;

            uint32_t slotIndex = handle.GetIndex();
            if (slotIndex >= m_capacity) return;

            Slot& slot = m_slots[slotIndex];
            if (slot.generation == handle.GetGeneration()) {
                slot.generation++;
                m_freeSlotsIndices.Push(slotIndex);
            }
        }

        ResourceType* Get(HandleType handle) {
            if (!handle.IsValid()) return nullptr;

            uint32_t slotIndex = handle.GetIndex();
            if (slotIndex >= m_capacity) return nullptr;

            Slot& slot = m_slots[slotIndex];
            if (slot.generation != handle.GetGeneration()) return nullptr;

            return &slot.resource;
        }

    private:
        struct Slot {
            ResourceType resource;
            uint32_t generation = 1;
        };

        uint32_t m_capacity;
        std::unique_ptr<Slot[]> m_slots;
        FixedStack<uint32_t> m_freeSlotsIndices;
    };
}
