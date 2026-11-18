

# File Engine.h

[**File List**](files.md) **>** [**dalia**](dir_7ee0a0312321b1cfc250dbf199abb419.md) **>** [**Engine.h**](Engine_8h.md)

[Go to the documentation of this file](Engine_8h.md)


```C++
#pragma once


#include "dalia/Result.h"
#include "dalia/Log.h"
#include "dalia/Device.h"
#include "dalia/Playback.h"
#include "dalia/Sound.h"
#include "dalia/Effect.h"

namespace dalia {

    struct EngineInternalState;

    struct EngineConfig {
        uint32_t sampleRate = 48000; // Internal mixing sample rate
        const char* startupOutputDeviceId = "default"; // Output device that should attempt to start on initialization.

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
        uint32_t streamCapacity     = 32;  // The maximum number of playback instances playing stream sounds at once.
        uint32_t busCapacity        = 64;  // The maximum number of buses that can exist at once.

        // Effect Capacities
        uint32_t biquadCapacity = 32; // The maximum number of biquad filters that can exist at once.

        struct Advanced {
            uint32_t periodSizeFrames = 512; // The number of frames per audio period.

            // Internal Messaging Queue Capacities (must be power of 2)
            size_t mixerQueueCapacity   = 1024;
            size_t streamQueueCapacity  = 256;
            size_t loadQueueCapacity    = 128;
            size_t controlQueueCapacity = 32;
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

        Result Initialize(const EngineConfig& config = EngineConfig{});

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
        // [ DEVICE MANAGEMENT ]
        // Methods for handling output devices.
        // ============================================================================
#pragma region DEVICE_MANAGEMENT

        Result GetOutputDeviceCount(uint32_t& count) const;

        Result GetOutputDeviceInfo(uint32_t index, OutputDeviceInfo& info) const;

        Result GetActiveOutputDeviceInfo(OutputDeviceInfo& info) const;

        Result GetTargetOutputDeviceId(char* identifier, size_t maxLength) const;

        Result SetOutputDevice(const char* identifier);

#pragma endregion ENGINE_DEVICE

        // ============================================================================
        // [ ASSET MANAGEMENT ]
        // Lifecycle methods for loading, tracking, and freeing raw audio memory
        // and soundbanks.
        // ============================================================================
#pragma region ASSET_MANAGEMENT

        Result LoadSoundAsync(SoundHandle& sound, SoundType type, const char* filepath,
            AssetLoadCallback loadCallback = nullptr, uint32_t* requestId = nullptr);

        Result UnloadSound(SoundHandle sound);

        Result GetSoundLength(SoundHandle sound, double& lengthInSeconds);

#pragma endregion ASSET_MANAGEMENT

        // ============================================================================
        // [ BUS MANAGEMENT ]
        // Lifecycle methods for creating, destroying, routing and modifying buses.
        // ============================================================================
#pragma region BUS_MANAGEMENT

        Result CreateBus(const char* bus, const char* parentBus = "Master");

        Result DestroyBus(const char* bus);

        Result RouteBus(const char* bus, const char* parentBus);

        Result SetBusVolumeDb(const char* bus, float volumeDb);

#pragma endregion BUS_MANAGEMENT

        // ============================================================================
        // [ EFFECTS MANAGEMENT ]
        // Lifecycle methods for creating, destroying, attaching, detaching and
        // modifying effects.
        // ============================================================================
#pragma region EFFECTS_MANAGEMENT

        template <typename TParams>
        Result CreateEffect(EffectHandle& effect, const TParams& params);

        Result DestroyEffect(EffectHandle effect);

        Result AttachEffect(EffectHandle effect, const char* bus, uint32_t effectSlot);

        Result DetachEffect(EffectHandle effect);

        template <typename TParams>
        Result SetEffectParams(EffectHandle effect, const TParams& params);

#pragma endregion EFFECTS_MANAGEMENT

        // ============================================================================
        // [ PLAYBACK MANAGEMENT ]
        // Methods for creating and modifying playback instances in terms of state
        // and parameters.
        // ============================================================================
#pragma region PLAYBACK_MANAGEMENT

        Result CreatePlayback(PlaybackHandle& playback, SoundHandle sound, PlaybackExitCallback exitCallback = nullptr);

        Result RoutePlayback(PlaybackHandle playback, const char* bus);

        Result PlayPlayback(PlaybackHandle playback);

        Result PausePlayback(PlaybackHandle playback);

        Result StopPlayback(PlaybackHandle playback);

        Result SeekPlayback(PlaybackHandle playback, double timeInSeconds);

        Result SetPlaybackVolumeDb(PlaybackHandle playback, float volumeDb);

        Result SetPlaybackRate(PlaybackHandle playback, float rate);

        Result SetPlaybackStereoPan(PlaybackHandle playback, float stereoPan);

        Result SetPlaybackLooping(PlaybackHandle playback, bool looping);

        Result SetPlaybackSpatial(PlaybackHandle playback, bool spatial);

        Result SetPlaybackPosition(PlaybackHandle playback, const Vec3& position);

        Result SetPlaybackVelocity(PlaybackHandle playback, const Vec3& velocity);

        Result SetPlaybackDistanceMode(PlaybackHandle playback, DistanceMode distanceMode);

        Result SetPlaybackAttenuationCurve(PlaybackHandle playback, AttenuationCurve curve);

        Result SetPlaybackMinMaxDistance(PlaybackHandle playback, float minDistance, float maxDistance);

        Result SetPlaybackUseDoppler(PlaybackHandle playback, bool useDoppler);

        Result SetPlaybackDopplerFactor(PlaybackHandle playback, float dopplerFactor);

        Result SetPlaybackListenerMask(PlaybackHandle playback, ListenerMask listenerMask);

#pragma endregion PLAYBACK_MANAGEMENT

        // ============================================================================
        // [ LISTENER MANAGEMENT ]
        // Methods for activating, deactivating and modifying listeners.
        // ============================================================================
#pragma region LISTENER_MANAGEMENT

        Result SetListenerActive(uint32_t listenerIndex, bool active);

        Result SetListenerPosition(uint32_t listenerIndex, const Vec3& position);

        Result SetListenerDistanceProbePosition(uint32_t listenerIndex, const Vec3& position);

        Result SetListenerVelocity(uint32_t listenerIndex, const Vec3& velocity);

        Result SetListenerOrientation(uint32_t listenerIndex, const Vec3& forward, const Vec3& up);

        Result SetListener3DAttributes(uint32_t listenerIndex, const Listener3DAttributes& attributes);

#pragma endregion LISTENER_MANAGEMENT

    private:
        EngineInternalState* m_state = nullptr;
    };
}
```


