#pragma once

#include "core/FixedStack.h"

#include <memory>
#include <cstdint>

#include "FixedStack.h"

namespace dalia {

    class HandleManager {
    public:
        HandleManager(uint32_t capacity);
        ~HandleManager() = default;

        bool Allocate(uint32_t& outIndex, uint32_t& outGeneration);
        bool Free(uint32_t index, uint32_t generation);

        [[nodiscard]] bool IsValid(uint32_t index, uint32_t generation) const;

        uint32_t GetCapacity() const;

    private:
        uint32_t m_capacity = 0;

        std::unique_ptr<bool[]> m_isUsed;
        std::unique_ptr<uint32_t[]> m_generations;
        FixedStack<uint32_t> m_freeIndices;
    };
}
