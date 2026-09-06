#pragma once

/// @file SoundControl.h
/// @brief Asset management and async load callbacks.

#include "Result.h"

#include <cstdint>
#include <functional>

namespace dalia {

	/// @brief The maximum string length (including null-terminator) of a filepath.
	constexpr size_t MAX_STR_LEN_ASSET_PATH = 256;

	/// @brief Specifies the memory management and playback strategy for an audio asset.
    enum class SoundType : uint8_t {
        None     = 0,
        Resident = 1, // The entire audio file is decompressed and loaded into RAM.
        Stream   = 2 // The audio data is read from disk in chunks.
    };

    /// @brief Handle used to manage loaded sounds. This handle expires once the sound it is referencing is unloaded.
    struct SoundHandle {
    public:
    	SoundHandle() = default;
    	explicit SoundHandle(uint64_t rawId) : m_rawId(rawId) {}

    	/// @return true if the handle has referenced a loaded sound at some point. Otherwise, false.
        bool IsValid() const { return m_rawId != 0; }
        bool operator==(const SoundHandle& other) const { return m_rawId == other.m_rawId; }
        bool operator!=(const SoundHandle& other) const { return m_rawId != other.m_rawId; }

    	/// @return The type of sound (stream or resident) that this handle references.
    	SoundType GetType() const { return static_cast<SoundType>(m_rawId >> 56); }

    	/// @return The underlying raw id of the handle.
        uint64_t GetRawId() const { return m_rawId; }

    private:
        friend struct EngineInternalState;
        friend class AssetRegistry;
        friend class AsyncLoadSystem;

        static SoundHandle Create(uint32_t index, uint32_t generation, SoundType type) {
            SoundHandle handle;
            uint64_t typeBits = static_cast<uint64_t>(type) << 56;
            uint64_t generationBits = (static_cast<uint64_t>(generation) & 0xFFFFFF) << 32;
            uint64_t indexBits = static_cast<uint64_t>(index);

            handle.m_rawId = typeBits | generationBits | indexBits;
            return handle;
        }

        static SoundHandle FromRawId(uint64_t rawId) {
            SoundHandle handle;
            handle.m_rawId = rawId;
            return handle;
        }

    	uint32_t GetIndex() const { return static_cast<uint32_t>(m_rawId & 0xFFFFFFFF); }
    	uint32_t GetGeneration() const { return static_cast<uint32_t>((m_rawId >> 32) & 0xFFFFFF); }

        uint64_t m_rawId = 0;
    };

	/// @brief Sentinel value indicating a failed async request.
    constexpr uint32_t INVALID_REQUEST_ID = 0;

    /// @brief Defines the signature for asset load callbacks.
    ///
    /// If provided when requesting an asset load, this function executes when the asynchronous load completes. This
    /// happens regardless if the load operation has succeeded or failed. Make sure to check the populated result code.
    ///
    /// @param requestId	The (optionally provided) identifier populated by the engine when the load was requested.
    /// @param result		The outcome of the load operation.
    using AssetLoadCallback = std::function<void(uint32_t requestId, Result result)>;
}