#pragma once
#include <cstdint>

// This class is currently unused (Will be used later for soundbanks and possibly events)

namespace dalia {

    class AssetRegistry;
    template <typename T, typename H> class HandlePool;

    template <typename Tag>
    struct ResourceHandle {
    public:
        bool IsValid() const { return uuid != 0; }
        bool operator==(const ResourceHandle& other) const { return uuid == other.uuid; }
        bool operator!=(const ResourceHandle& other) const { return uuid != other.uuid; }

        uint64_t GetUUID() const { return uuid; }

        static ResourceHandle FromUUID(uint64_t rawUuid) {
            ResourceHandle handle;
            handle.uuid = rawUuid;
            return handle;
        }

    private:
        friend class AssetRegistry;
        template <typename T, typename H> friend class HandlePool;

        static ResourceHandle Create(uint32_t index, uint32_t generation) {
            ResourceHandle handle;
            handle.uuid = (static_cast<uint64_t>(generation) << 32) | index;
            return handle;
        }

        uint32_t GetIndex() const { return static_cast<uint32_t>(uuid & 0xFFFFFFFF); }
        uint32_t GetGeneration() const { return static_cast<uint32_t>(uuid >> 32); }

        uint64_t uuid = 0;
    };

    struct BankTag {};

    // The actual handle types exposed to the engine
    using BankHandle          = ResourceHandle<BankTag>;
}