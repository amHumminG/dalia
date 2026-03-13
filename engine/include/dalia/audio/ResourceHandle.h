#pragma once
#include <cstdint>

namespace dalia {

    class AssetRegistry;
    template <typename T, typename H, uint32_t C> class HandlePool;

    template <typename Tag>
    struct ResourceHandle {
    public:
        bool IsValid() const { return uuid != 0; }
        bool operator==(const ResourceHandle& other) const { return uuid == other.uuid; }
        bool operator!=(const ResourceHandle& other) const { return uuid != other.uuid; }

    private:
        friend class AssetRegistry;
        template <typename T, typename H, uint32_t C> friend class HandlePool;

        static ResourceHandle Create(uint32_t index, uint32_t generation) {
            ResourceHandle handle;
            handle.uuid = (static_cast<uint64_t>(generation) << 32) | index;
            return handle;
        }

        uint32_t GetIndex() const { return static_cast<uint32_t>(uuid & 0xFFFFFFFF); }
        uint32_t GetGeneration() const { return static_cast<uint32_t>(uuid >> 32); }

        uint64_t uuid = 0;

        struct ResidentTag {};
        struct StreamTag {};
        struct BankTag {};

        // The actual handle types exposed to the engine
        using ResidentHandle = ResourceHandle<ResidentTag>;
        using StreamHandle   = ResourceHandle<StreamTag>;
        using BankHandle     = ResourceHandle<BankTag>;
    };
}