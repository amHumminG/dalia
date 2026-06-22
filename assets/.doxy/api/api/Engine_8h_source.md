

# File Engine.h

[**File List**](files.md) **>** [**audio**](dir_0ee167d633b723baeec4094afeaf5d43.md) **>** [**Engine.h**](Engine_8h.md)

[Go to the documentation of this file](Engine_8h.md)


```C++
#pragma once

#include "dalia/core/Result.h"
#include "dalia/core/LogLevel.h"

#include "dalia/audio/PlaybackControl.h"
#include "dalia/audio/SoundControl.h"
#include "dalia/audio/EffectControl.h"

namespace dalia {

    struct EngineInternalState;

    struct EngineConfig {
        LogLevel logLevel = LogLevel::Warning;
        LogCallback logCallback = nullptr;

        CoordinateSystem coordinateSystem = CoordinateSystem::RightHanded;

        uint32_t residentSoundCapacity = 256;
        uint32_t streamSoundCapacity = 256;

        uint32_t voiceCapacity      = 128;
        uint32_t maxActiveVoices    = 64; // Currently unused
        uint32_t streamCapacity     = 32;
        uint32_t busCapacity        = 64;

        // Effects
        uint32_t BiquadCapacity     = 32;

        uint32_t listenerCapacity   = 1; // Min 1, max 4

        size_t rtCommandQueueCapacity       = 1024;
        size_t rtEventQueueCapacity         = 1024;
        size_t ioStreamRequestQueueCapacity = 256;
        size_t ioLoadRequestQueueCapacity   = 64;
        size_t ioLoadEventQueueCapacity     = 64;
    };

    class Engine {
    public:
        Engine();
        ~Engine();

        Engine(const Engine&) = delete;
        Engine& operator=(const Engine&) = delete;

        // ============================================================================
        // [ ENGINE LIFECYCLE ]
        // Core methods for initializing, ticking, shutting down the engine.
        // ============================================================================
#pragma region ENGINE_LIFECYCLE

        Result Init(const EngineConfig& config = EngineConfig{});

        Result Shutdown();

        void Update();

#pragma endregion ENGINE_LIFECYCLE

        // ============================================================================
        // [ ENGINE SETTINGS ]
        // Core methods for setting engine attributes.
        // ============================================================================
#pragma region ENGINE_SETTINGS

        Result SetGlobalDopplerFactor(float globalDopplerFactor);

#pragma endregion ENGINE_SETTINGS

        // ============================================================================
        // [ ASSET MANAGEMENT ]
        // Lifecycle methods for loading, tracking, and freeing raw audio memory
        // and soundbanks.
        // ============================================================================
#pragma region ASSET_MANAGEMENT

        Result LoadSoundAsync(SoundHandle& sound, SoundType type, const char* filepath,
            AssetLoadCallback callback = nullptr, uint32_t* outRequestId = nullptr);

        Result UnloadSound(SoundHandle sound);

        Result GetSoundLength(SoundHandle sound, double& lengthInSeconds);

#pragma endregion ASSET_MANAGEMENT

        // ============================================================================
        // [ BUS MANAGEMENT ]
        // Lifecycle methods for creating, destroying, routing and modifying buses.
        // ============================================================================
#pragma region BUS_MANAGEMENT

        Result CreateBus(const char* identifier, const char* parentIdentifier = "Master");

        Result DestroyBus(const char* identifier);

        Result RouteBus(const char* identifier, const char* parentIdentifier);

        Result SetBusVolumeDb(const char* identifier, float volumeDb);

#pragma endregion BUS_MANAGEMENT

        // ============================================================================
        // [ EFFECTS MANAGEMENT ]
        // Lifecycle methods for creating, destroying, attaching, detaching and
        // modifying effects.
        // ============================================================================
#pragma region EFFECTS_MANAGEMENT

        template <typename TParams>
        requires requires(TParams p) {p.Sanitize(); }
        Result CreateEffect(EffectHandle& effect, const TParams& params);

        template <typename TParams>
        requires requires(TParams p) {p.Sanitize(); }
        Result SetEffectParams(EffectHandle effect, const TParams& params);

        Result AttachEffect(EffectHandle effect, const char* busIdentifier, uint32_t effectSlot);

        Result DetachEffect(EffectHandle effect);

        Result DestroyEffect(EffectHandle effect);

#pragma endregion EFFECTS_MANAGEMENT

        // ============================================================================
        // [ PLAYBACK MANAGEMENT ]
        // Methods for creating and modifying playback instances in terms of state
        // and parameters.
        // ============================================================================
#pragma region PLAYBACK_MANAGEMENT

        Result CreatePlayback(PlaybackHandle& playback, SoundHandle sound,
                              PlaybackExitCallback callback = nullptr);

        Result RoutePlayback(PlaybackHandle playback, const char* busIdentifier);

        Result PlayPlayback(PlaybackHandle playback);

        Result PausePlayback(PlaybackHandle playback);

        Result StopPlayback(PlaybackHandle playback);

        Result SeekPlayback(PlaybackHandle playback, double timeInSeconds);

        Result SetPlaybackVolumeDb(PlaybackHandle playback, float volumeDb);

        Result SetPlaybackRate(PlaybackHandle playback, float rate);

        Result SetPlaybackStereoPan(PlaybackHandle playback, float pan);

        Result SetPlaybackLooping(PlaybackHandle playback, bool looping);

        Result SetPlaybackSpatial(PlaybackHandle playback, bool spatial);

        Result SetPlaybackDistanceMode(PlaybackHandle playback, DistanceMode mode);

        Result SetPlaybackAttenuationCurve(PlaybackHandle playback, AttenuationCurve curve);

        Result SetPlaybackPosition(PlaybackHandle playback, const Vec3& position);

        Result SetPlaybackMinMaxDistance(PlaybackHandle playback, float minDistance, float maxDistance);

        Result SetPlaybackUseDoppler(PlaybackHandle playback, bool useDoppler);

        Result SetPlaybackDopplerFactor(PlaybackHandle playback, float dopplerFactor);

        Result SetPlaybackVelocity(PlaybackHandle playback, const Vec3& velocity);

        Result SetPlaybackListenerMask(PlaybackHandle playback, ListenerMask mask);

#pragma endregion PLAYBACK_MANAGEMENT

        // ============================================================================
        // [ LISTENER MANAGEMENT ]
        // Methods for activating, deactivating and modifying listeners.
        // ============================================================================
#pragma region LISTENER_MANAGEMENT

        Result SetListenerActive(uint32_t listenerIndex, bool active);

        Result SetListener3DAttributes(uint32_t listenerIndex, const Listener3DAttributes& attributes);

        Result SetListenerPosition(uint32_t listenerIndex, const Vec3& position);

        Result SetListenerDistanceProbePosition(uint32_t listenerIndex, const Vec3& distanceProbePosition);

        Result SetListenerOrientation(uint32_t listenerIndex, const Vec3& forward, const Vec3& up);

        Result SetListenerVelocity(uint32_t listenerIndex, const Vec3& velocity);

#pragma endregion LISTENER_MANAGEMENT

    private:
        void TeardownInternal();

        EngineInternalState* m_state = nullptr;

    };
}
```


