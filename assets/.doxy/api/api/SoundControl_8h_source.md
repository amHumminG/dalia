

# File SoundControl.h

[**File List**](files.md) **>** [**audio**](dir_0ee167d633b723baeec4094afeaf5d43.md) **>** [**SoundControl.h**](SoundControl_8h.md)

[Go to the documentation of this file](SoundControl_8h.md)


```C++
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
        SoundHandle() = default;
        explicit SoundHandle(uint64_t rawId) : m_rawId(rawId) {}

        bool IsValid() const { return m_rawId != 0; }
        bool operator==(const SoundHandle& other) const { return m_rawId == other.m_rawId; }
        bool operator!=(const SoundHandle& other) const { return m_rawId != other.m_rawId; }

        SoundType GetType() const { return static_cast<SoundType>(m_rawId >> 56); }

        uint64_t GetRawId() const { return m_rawId; }

    private:
        friend struct EngineInternalState;
        friend class AssetRegistry;
        friend class IoLoadSystem;

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

    constexpr uint32_t INVALID_REQUEST_ID = 0;

    using AssetLoadCallback = std::function<void(uint32_t requestId, Result result)>;
}
```


