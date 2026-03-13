#pragma once
#include "core/FixedStack.h"
#include <memory>
#include <cstdint>

namespace dalia {

    template <typename ResourceType, typename HandleType, uint32_t Capacity>
    class HandlePool {
    public:
        HandlePool()
            : m_slots(std::make_unique<Slot[]>(Capacity)),
            m_freeSlotsIndices(Capacity) {

            for (int i = Capacity - 1; i >= 0; i--) {
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
            if (slotIndex >= Capacity) return;

            Slot& slot = m_slots[slotIndex];
            if (slot.generation == handle.GetGeneration()) {
                slot.generation++;
                m_freeSlotsIndices.Push(slotIndex);
            }
        }

        ResourceType* Get(HandleType handle) {
            if (!handle.IsValid()) return nullptr;

            uint32_t slotIndex = handle.GetIndex();
            if (slotIndex >= Capacity) return nullptr;

            Slot& slot = m_slots[slotIndex];
            if (slot.generation != handle.GetGeneration()) return nullptr;

            return &slot.resource;
        }

    private:
        struct Slot {
            ResourceType resource;
            uint32_t generation = 1;
        };

        std::unique_ptr<Slot[]> m_slots;
        FixedStack<uint32_t> m_freeSlotsIndices;
    };
}
