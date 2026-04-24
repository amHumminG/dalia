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

    /// @brief Handle used to manage loaded sounds. This handle expires once the sound it is referencing is unloaded.
    struct SoundHandle {
    public:
    	/// @return true if the handle has referenced a loaded sound at some point. Otherwise, false.
        bool IsValid() const { return rawId != 0; }
        bool operator==(const SoundHandle& other) const { return rawId == other.rawId; }
        bool operator!=(const SoundHandle& other) const { return rawId != other.rawId; }

    	/// @return The type of sound (stream or resident) that this handle references.
    	SoundType GetType() const { return static_cast<SoundType>(rawId >> 56); }

    	/// @return The underlying raw id of the handle.
        uint64_t GetRawId() const { return rawId; }

    private:
        friend struct EngineInternalState;
        friend class AssetRegistry;
        friend class IoLoadSystem;

        static SoundHandle Create(uint32_t index, uint32_t generation, SoundType type) {
            SoundHandle handle;
            uint64_t typeBits = static_cast<uint64_t>(type) << 56;
            uint64_t generationBits = (static_cast<uint64_t>(generation) & 0xFFFFFF) << 32;
            uint64_t indexBits = static_cast<uint64_t>(index);

            handle.rawId = typeBits | generationBits | indexBits;
            return handle;
        }

        static SoundHandle FromRawId(uint64_t rawId) {
            SoundHandle handle;
            handle.rawId = rawId;
            return handle;
        }

    	uint32_t GetIndex() const { return static_cast<uint32_t>(rawId & 0xFFFFFFFF); }
    	uint32_t GetGeneration() const { return static_cast<uint32_t>((rawId >> 32) & 0xFFFFFF); }

        uint64_t rawId = 0;
    };

    constexpr uint32_t INVALID_REQUEST_ID = 0;

    /// @brief A function that, if provided when loading an asset, will be called once the asset has been successfully
    /// loaded.
    using AssetLoadCallback = std::function<void(uint32_t requestId, Result result)>;
}