#pragma once

#include "dalia/core/Result.h"

#include <cstdint>
#include <functional>

namespace dalia {

    enum class SoundType : uint8_t {
        None     = 0,
        Resident = 1,
        Stream   = 2
    };

    struct SoundHandle {
    public:
        bool IsValid() const { return uuid != 0; }
        bool operator==(const SoundHandle& other) const { return uuid == other.uuid; }
        bool operator!=(const SoundHandle& other) const { return uuid != other.uuid; }

        SoundType GetType() const { return static_cast<SoundType>(uuid >> 56); }
        uint32_t GetIndex() const { return static_cast<uint32_t>(uuid & 0xFFFFFFFF); }
        uint32_t GetGeneration() const { return static_cast<uint32_t>((uuid >> 32) & 0xFFFFFF); }

        uint64_t GetUUID() const { return uuid; }

    private:
        friend struct EngineInternalState;
        friend class AssetRegistry;
        friend class IoLoadSystem;

        static SoundHandle Create(uint32_t index, uint32_t generation, SoundType type) {
            SoundHandle handle;
            uint64_t typeBits = static_cast<uint64_t>(type) << 56;
            uint64_t generationBits = (static_cast<uint64_t>(generation) & 0xFFFFFF) << 32;
            uint64_t indexBits = static_cast<uint64_t>(index);

            handle.uuid = typeBits | generationBits | indexBits;
            return handle;
        }

        static SoundHandle FromUUID(uint64_t rawUuid) {
            SoundHandle handle;
            handle.uuid = rawUuid;
            return handle;
        }

        uint64_t uuid = 0;
    };

    constexpr uint32_t INVALID_REQUEST_ID = 0;
    using AssetLoadCallback = std::function<void(uint32_t requestId, Result result)>;
}