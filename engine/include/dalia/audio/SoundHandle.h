#pragma once
#include <cstdint>

namespace dalia {

    class AssetRegistry;

    enum class SoundType : uint8_t {
        Resident = 0,
        Stream   = 1
    };

    struct SoundHandle {
    public:
        bool IsValid() const { return uuid != 0; }
        bool operator==(const SoundHandle& other) const { return uuid == other.uuid; }
        bool operator!=(const SoundHandle& other) const { return uuid != other.uuid; }

        SoundType GetType() const { return static_cast<SoundType>(uuid >> 56); }

        uint64_t GetUUID() const { return uuid; }

        static SoundHandle FromUUID(uint64_t rawUuid) {
            SoundHandle handle;
            handle.uuid = rawUuid;
            return handle;
        }

    private:
        friend class AssetRegistry;

        static SoundHandle Create(uint32_t index, uint32_t generation, SoundType type) {
            SoundHandle handle;
            uint64_t typeBits = static_cast<uint64_t>(type) << 56;
            uint64_t generationBits = (static_cast<uint64_t>(generation) & 0xFFFFFF) << 32;
            uint64_t indexBits = static_cast<uint64_t>(index);

            handle.uuid = typeBits | generationBits | indexBits;
            return handle;
        }

        uint32_t GetIndex() const { return static_cast<uint32_t>(uuid & 0xFFFFFFFF); }
        uint32_t GetGeneration() const { return static_cast<uint32_t>((uuid >> 32) & 0xFFFFFF); }

        uint64_t uuid = 0;
    };
}