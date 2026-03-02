#pragma once

#include <cstdint>
#include <span>
#include <memory>

namespace dalia {

    struct BusMirror;
    template <typename T> class FixedStack;

    class BusGraphCompiler {
    public:
        explicit BusGraphCompiler(uint32_t busCapacity);
        ~BusGraphCompiler();

        uint32_t Compile(
            std::span<const BusMirror> busPoolMirror,
            std::span<uint32_t> outputMixOrder
        );

    private:
        uint32_t m_busCapacity;

        std::unique_ptr<uint32_t[]> m_pendingChildrenCount;
        std::unique_ptr<FixedStack<uint32_t>>   m_leaves;
    };
}