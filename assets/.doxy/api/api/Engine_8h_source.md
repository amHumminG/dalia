

# File Engine.h

[**File List**](files.md) **>** [**audio**](dir_0ee167d633b723baeec4094afeaf5d43.md) **>** [**Engine.h**](Engine_8h.md)

[Go to the documentation of this file](Engine_8h.md)


```C++
#pragma once

#include "dalia/core/Result.h"
#include "dalia/core/LogLevel.h"

#include "dalia/audio/DeviceControl.h"
#include "dalia/audio/PlaybackControl.h"
#include "dalia/audio/SoundControl.h"
#include "dalia/audio/EffectControl.h"

namespace dalia {

    struct EngineInternalState;

    struct EngineConfig {
        // Core Settings
        uint32_t sampleRate = 48000; // Internal mixing sample rate

        // Logging
        LogLevel logLevel = LogLevel::Warning; // The level at which the engine will log messages.
        LogCallback logCallback = nullptr; // Optional log sink.

        // Spatialization
        CoordinateSystem coordinateSystem = CoordinateSystem::RightHanded; // The coordinate system used for spatial audio.
        uint32_t listenerCapacity = 1; // The maximum number of listeners that can exist at once (min=1 max=4).

        // Asset Capacities
        uint32_t residentSoundCapacity  = 256; // The number of resident sounds the engine is able hold loaded at once
        uint32_t streamSoundCapacity    = 256; // The number of stream sounds the engine is able to hold loaded at once

        // Mixing Capacities
        uint32_t voiceCapacity      = 128; // The maximum number of playback instances that can exist at once.
        uint32_t streamCapacity     = 32; // The maximum number of playback instances playing stream sounds at once.
        uint32_t busCapacity        = 64; // The maximum number of buses that can exist at once.

        // Effect Capacities
        uint32_t BiquadCapacity = 32; // The maximum number of biquad filters that can exist at once.

        struct Advanced {
            // Internal Messaging Queue Capacities (must be power of 2)
            size_t RealTimeQueueCapacity        = 1024;
            size_t AsyncStreamQueueCapacity     = 256;
            size_t AsyncLoadQueueCapacity       = 128;
            size_t AsyncControlQueueCapacity    = 32;
        } advanced;
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

        Result GetOutputDeviceCount(uint32_t& count) const;

        Result GetOutputDeviceInfo(uint32_t index, OutputDeviceInfo& info) const;

        Result GetActiveOutputDeviceInfo(OutputDeviceInfo& info) const;

        Result GetTargetOutputDeviceId(char* identifier, size_t maxLength) const;

        Result SetOutputDevice(const char* identifier);

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


