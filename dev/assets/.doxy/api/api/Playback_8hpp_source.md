

# File Playback.hpp

[**File List**](files.md) **>** [**dalia**](dir_7ee0a0312321b1cfc250dbf199abb419.md) **>** [**Playback.hpp**](Playback_8hpp.md)

[Go to the documentation of this file](Playback_8hpp.md)


```C++
#pragma once


#include <cstdint>
#include <functional>

namespace dalia {

    struct Vec3 {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
    };

    struct PlaybackHandle {
    public:
        PlaybackHandle() = default;

        explicit PlaybackHandle(uint64_t rawId)
            : m_rawId(rawId) {
        }

        bool IsValid() const {
            return m_rawId != 0;
        }

        bool operator==(const PlaybackHandle& other) const {
            return m_rawId == other.m_rawId;
        }

        bool operator!=(const PlaybackHandle& other) const {
            return m_rawId != other.m_rawId;
        }

        uint64_t GetRawId() const {
            return m_rawId;
        }

    private:
        friend class Engine;
        friend class MixController;

        static PlaybackHandle Create(uint32_t index, uint32_t generation) {
            PlaybackHandle handle;
            handle.m_rawId = (static_cast<uint64_t>(generation) << 32) | index;
            return handle;
        }

        uint32_t GetIndex() const {
            return static_cast<uint32_t>(m_rawId & 0xFFFFFFFF);
        }

        uint32_t GetGeneration() const {
            return static_cast<uint32_t>(m_rawId >> 32);
        }

        uint64_t m_rawId = 0;
    };

    constexpr PlaybackHandle INVALID_PLAYBACK_HANDLE{};

    enum class PlaybackExitCondition : uint8_t {
        None            = 0, 
        NaturalEnd      = 1, 
        ExplicitStop    = 2, 
        Evicted         = 3, 
        Error           = 4, 
    };

    using PlaybackExitCallback = std::function<void(PlaybackHandle playback, PlaybackExitCondition exitCondition)>;

    enum class AttenuationCurve : uint8_t {
        InverseSquare   = 0, 
        Linear          = 1, 
        Quadratic       = 2  
    };

    enum class DistanceMode : uint8_t {
        FromListener        = 0, 
        FromDistanceProbe   = 1  
    };

    struct Listener3DAttributes {
        Vec3 position{0.0f, 0.0f, 0.0f};                
        Vec3 distanceProbePosition{0.0f, 0.0f, 0.0f};   

        Vec3 forward{0.0f, 0.0f, 1.0f};             
        Vec3 up{0.0f, 1.0f, 0.0f};                      

        Vec3 velocity{0.0f, 0.0f, 0.0f};                
    };

    inline constexpr Listener3DAttributes MakeListener3DAttributes(
        const Vec3& position,
        const Vec3& forward,
        const Vec3& up,
        const Vec3& velocity) {
        return {position, position, forward, up, velocity};
    }

    inline constexpr Listener3DAttributes MakeListener3DAttributesSplit(
        const Vec3& position,
        const Vec3& probePosition,
        const Vec3& forward,
        const Vec3& up,
        const Vec3& velocity) {
        return {position, probePosition, forward, up, velocity};
    }

    using ListenerMask = uint32_t;

    constexpr ListenerMask MASK_NONE          = 0b00000000; 
    constexpr ListenerMask MASK_LISTENER_0    = 0b00000001; 
    constexpr ListenerMask MASK_LISTENER_1    = 0b00000010; 
    constexpr ListenerMask MASK_LISTENER_2    = 0b00000100; 
    constexpr ListenerMask MASK_LISTENER_3    = 0b00001000; 
    constexpr ListenerMask MASK_ALL_LISTENERS = 0xFFFFFFFF; 

    inline constexpr ListenerMask MakeListenerMask(uint32_t listenerIndex) {
        return (1 << listenerIndex);
    }

    enum class CoordinateSystem : uint8_t {
        RightHanded = 0, 
        LeftHanded  = 1  
    };
}
```


