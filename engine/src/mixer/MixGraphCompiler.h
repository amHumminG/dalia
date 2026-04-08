#pragma once

#include <cstdint>
#include <span>
#include <memory>

namespace dalia {

	struct Bus;
    template <typename T> class FixedStack;

    class MixGraphCompiler {
    public:
        explicit MixGraphCompiler(uint32_t busCapacity);
        ~MixGraphCompiler();

        uint32_t Compile(
            std::span<const Bus> busPool,
            std::span<uint32_t> outMixOrder
        );

    private:
        uint32_t m_busCapacity;

        std::unique_ptr<uint32_t[]> m_pendingChildrenCount;
        std::unique_ptr<FixedStack<uint32_t>>   m_leaves;
    };
}