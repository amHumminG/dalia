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

		uint32_t residentSoundCapacity = 256; // Maybe this should be higher?
		uint32_t streamSoundCapacity = 256;

		uint32_t voiceCapacity		= 128;
		uint32_t maxActiveVoices	= 64;
		uint32_t streamCapacity		= 32;
		uint32_t busCapacity		= 64;
		uint32_t biquadCapacity		= 32;

		uint32_t listenerCapacity	= 1; // Min 1, max 4

		size_t rtCommandQueueCapacity		= 1024;
		size_t rtEventQueueCapacity			= 1024;
		size_t ioStreamRequestQueueCapacity	= 256;
		size_t ioLoadRequestQueueCapacity	= 64;
		size_t ioLoadEventQueueCapacity		= 64;
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

		/// @brief Initializes the engine, hardware device and background threads.
		///
		/// This function allocates all internal memory pools and starts the background audio thread, I/O loading
		/// thread, and I/O streaming thread. It must be called once before any other engine methods are used.
		///
		/// @note [Hardware Device] The engine will default to the OS's default audio device.
		///
		/// @param[in] config The initialization settings for the engine.
		///
		/// @retval Result::Ok					The engine was successfully initialized and is ready for use.
		/// @retval Result::AlreadyInitialized	Attempted to initialize an already initialized engine.
		/// @retval Result::DeviceFailed		The operating system failed to initialize or start the audio device.
		Result Init(const EngineConfig& config);

		/// @brief Halts all audio processing, terminates all background threads and frees allocated engine memory.
		///
		/// @note [Blocking Operation] This method will block the calling thread until all background threads have
		/// fully joined.
		///
		/// @note [Handle Invalidation] Calling this method will invalidate all handles.
		///
		/// @retval Result::Ok				The engine was successfully shut down.
		/// @retval Result::NotInitialized	Attempted to shut down an engine that is not initialized.
		/// @retval Result::DeviceFailed	The operating system failed to release the hardware device.
		Result Shutdown();

		/// @brief The per-frame tick method for the engine.
		///
		/// This method is the primary synchronization router between the API (calling) thread and the background
		/// threads. It must be called exactly once per frame inside the update loop of your main thread.
		///
		/// @note [Callback Execution] This method processes all events happening within the engine. It is within
		/// this method that any provided AudioEventCallback and AssetLoadCallback functions are invoked.
		void Update();

#pragma endregion ENGINE_LIFECYCLE

		// ============================================================================
		// [ ENGINE SETTINGS ]
		// Core methods for setting engine attributes.
		// ============================================================================
#pragma region ENGINE_SETTINGS

		/// @brief Sets the global scaling factor for the Doppler effect for the entire engine.
		///
		/// @param[in] globalDopplerFactor The global scaling factor. Clamped internally between 0.0 and 10.0.
		///
		/// @retval Result::Ok				The global scaling factor for the Doppler effect was successfully set.
		/// @retval Result::NotInitialized	The engine is not initialized.
		Result SetGlobalDopplerFactor(float globalDopplerFactor);

#pragma endregion ENGINE_SETTINGS

		// ============================================================================
		// [ ASSET MANAGEMENT ]
		// Lifecycle methods for loading, tracking, and freeing raw audio memory
		// and soundbanks.
		// ============================================================================
#pragma region ASSET_MANAGEMENT

		/// @brief Asynchronously loads a sound into memory or prepares a file for streaming.
		///
		/// This function returns immediately and does not block the calling thread. The provided sound is valid
		/// immediately if the call succeeds. If playback using the sound is attempted before the load is complete,
		/// the engine will safely defer the playback request internally.
		///
		/// @note [Reference Counting] Sounds are reference counted. If the requested filepath with the same SoundType
		/// is already loaded, this function increments a reference count and returns the already loaded sound.
		/// Because of this, one must also unload a sound the same number of times as it has been loaded in order to
		/// properly unload it from the engine.
		///
		/// @param[out] sound			The handle to the sound.
		/// @param[in]  type			Specifies if the sound should be fully decoded into memory (Resident) or
		///								streamed directly from disk (Stream).
		/// @param[in]  filepath		The string path to the audio asset.
		/// @param[in]  callback		Optional. If provided, this function will be called when the load finishes.
		/// @param[out] outRequestId	Optional. Populated with a unique ID for the async request. It will evaluate to
		///								INVALID_REQUEST_ID if the asset is already loaded.
		///
		/// @retval Result::Ok							The request was successfully queued for load or is already
		///												loaded.
		/// @retval Result::NotInitialized				The engine is not initialized.
		/// @retval Result::ResidentSoundPoolExhausted	The allocated capacity for resident sounds has been reached.
		/// @retval Result::StreamSoundPoolExhausted	The allocated capacity for stream sounds has been reached.
		Result LoadSoundAsync(SoundHandle& sound, SoundType type, const char* filepath,
			AssetLoadCallback callback = nullptr, uint32_t* outRequestId = nullptr);

		/// @brief Decrements the reference count of a sound and unloads it if the count reaches zero.
		///
		/// If the reference count of a sound reaches zero, all active playbacks using the sound will be stopped with
		/// the exit condition ExplicitStop. The engine will also immediately abort all pending playback requests with
		/// the same exit condition.
		///
		/// @param[in] soundHandle The handle to the sound.
		///
		/// @retval Result::Ok				The sound was successfully unloaded or its reference count was decremented.
		/// @retval Result::NotInitialized	The engine is not initialized.
		/// @retval Result::InvalidHandle	The sound handle is not recognized, or has already been freed.
		Result UnloadSound(SoundHandle soundHandle);

		// Result LoadBank(BankHandle& handle, StringID pathId);
		// Result Unload(BankHandle handle);

#pragma endregion ASSET_MANAGEMENT

		// ============================================================================
		// [ BUS MANAGEMENT ]
		// Lifecycle methods for creating, destroying, routing and modifying buses.
		// ============================================================================
#pragma region BUS_MANAGEMENT

		/// @brief Creates a new mixing bus.
		///
		/// @note [Reference Counting] Buses are reference counted via their string identifiers. If CreateBus is
		/// called for an identifier that already exists, the engine will increment its reference count. If the bus
		/// already exists, any new parentIdentifier provided in subsequent calls is ignored and a warning is logged.
		///
		/// @param[in] identifier		The unique string identifier of the bus.
		/// @param[in] parentIdentifier	Optional. The unique string identifier of the parent bus (defaults to Master).
		///
		/// @retval Result::Ok					The bus was successfully created or its reference count was incremented.
		/// @retval Result::NotInitialized		The engine is not initialized.
		/// @retval Result::BusNotFound			The parent bus does not exist.
		/// @retval Result::BusPoolExhausted	The allocated bus capacity has been reached.
		Result CreateBus(const char* identifier, const char* parentIdentifier = "Master");

		/// @brief Decrements the reference count of a bus and destroys it if the count reaches zero.
		///
		/// If the bus is destroyed, any child buses or active playbacks that are routed to this bus are orphaned.
		/// An orphaned bus can be rerouted to a new parent bus, but an orphaned voice will immediately be stopped
		/// by the engine with the exit condition ExplicitStop.
		///
		/// @note [Master Immunity] The Master bus is permanently allocated by the engine and cannot be destroyed.
		///
		/// @param[in] identifier The unique string identifier of the bus.
		///
		/// @retval Result::Ok				The bus was successfully destroyed or its reference count was decremented.
		/// @retval Result::NotInitialized	The engine is not initialized.
		/// @retval Result::BusNotFound		The bus does not exist or has already been destroyed.
		/// @retval Result::Error			Attempted to destroy Master bus.
		Result DestroyBus(const char* identifier);

		/// @brief Modifies the routing topology by changing the output destination of an existing bus.
		///
		/// Moves a bus and all of its attached children (buses and playbacks) to a new parent bus in the mixing
		/// hierarchy.
		///
		/// @note [Hierarchy Protection] The mixing hierarchy must always remain a strict directed acyclic graph (DAG).
		/// If an attempt is made to route a bus into itself or into one of its own descendants, this function
		/// will return an error. Additionally, the Master bus cannot be re-routed.
		///
		/// @param[in] identifier		The unique string identifier of the bus.
		/// @param[in] parentIdentifier	The unique string identifier of the new parent bus (must be an existing bus).
		///
		/// @retval Result::Ok				The bus was successfully re-routed.
		/// @retval Result::NotInitialized	The engine is not initialized.
		/// @retval Result::InvalidRouting	Attempted a route that would result in a cyclic bus hierarchy, or
		///									attempted to route Master.
		/// @retval Result::BusNotFound		Either the bus or the parent bus does not exist.
		Result RouteBus(const char* identifier, const char* parentIdentifier);

		/// @brief Sets the mixing volume of a bus in decibels (dB).
		///
		/// @param[in] identifier	The unique string identifier of the bus.
		/// @param[in] volumeDb		The new volume in decibels (dB). A value of 0.0 represents unity gain. The volume
		///							is automatically clamped to the engine's safe operating range
		///							(between -60.0dB and +12.0dB).
		///
		/// @retval Result::Ok				The mixing volume of the bus was successfully updated.
		/// @retval Result::NotInitialized	The engine is not initialized.
		/// @retval Result::BusNotFound		The bus does not exist.
		Result SetBusVolumeDb(const char* identifier, float volumeDb);

#pragma endregion BUS_MANAGEMENT

		// ============================================================================
		// [ EFFECTS MANAGEMENT ]
		// Lifecycle methods for creating, destroying, attaching, detaching and
		// modifying effects.
		// ============================================================================
#pragma region EFFECTS_MANAGEMENT

		/// @brief Creates a new Biquad filter effect.
		///
		/// The effect is unattached upon creation.
		///
		/// @param[out] effect	The handle to the effect.
		/// @param[in] type		The mathematical curve of the filter.
		/// @param[in] config	The initial DSP parameters of the filter. The DSP parameters are automatically clamped.
		///						Frequencies are clamped between 20Hz and 20kHz and Resonance is clamped between 0.1
		///						and 10.0.
		///
		/// @retval Result::Ok							The filter was successfully created.
		/// @retval Result::NotInitialized				The engine is not initialized.
		/// @retval Result::BiquadFilterPoolExhausted	The allocated Biquad filter capacity has been reached.
		Result CreateBiquadFilter(EffectHandle& effect, BiquadFilterType type, const BiquadConfig& config);

		/// @brief Updates the DSP parameters of an existing Biquad filter.
		///
		/// @param[in] effect The handle to the effect.
		/// @param[in] config The new DSP parameters for the Biquad filter.
		///
		/// @retval Result::Ok
		/// @retval Result::NotInitialized	The engine is not initialized.
		/// @retval Result::InvalidHandle	The effect handle is not recognized or is for a different effect type.
		/// @retval Result::ExpiredHandle	The effect handle points to an already destroyed effect.
		Result SetBiquadParams(EffectHandle effect, const BiquadConfig& config);

		/// @brief Inserts an allocated effect into the processing chain of a specified bus.
		///
		/// A bus can hold up to four effects at a time. Attached effects are processed sequentially from
		/// slot 0 to slot 3.
		///
		/// @note [Exclusive Routing] A single effect instance can only be attached to one bus at a time.
		/// Attaching an effect that is already attached to a bus will forcefully detach that effect from its old bus.
		/// Doing so is generally not recommended if the bus is being played through as it can potentially cause a pop
		/// in the audio output due to bypassing the built-in effect fade-out.
		///
		/// @note [Hot-Swap] If the specified slot on the target bus is already occupied by another effect, the
		/// existing effect will be forcefully detached and replaced by the new effect. Doing so is generally not
		/// recommended for the same reasons mentioned above.
		///
		/// @param[in] effect			The handle to the effect.
		/// @param[in] busIdentifier	The unique string identifier of the target bus.
		/// @param[in] effectSlot		The index in the effect rack of the bus (0 to 3).
		///
		/// @retval Result::Ok						The effect was successfully attached to the bus.
		/// @retval Result::NotInitialized			The engine is not initialized.
		/// @retval Result::InvalidHandle			The effect handle is not recognized.
		/// @retval Result::ExpiredHandle			The effect handle points to an already destroyed effect.
		/// @retval Result::BusNotFound				The target bus does not exist.
		/// @retval Result::InvalidEffectSlot		The requested slot exceeds the rack index limit.
		Result AttachEffect(EffectHandle effect, const char* busIdentifier, uint32_t effectSlot);

		/// @brief Removes an effect from the mix graph, regardless of which bus it is routed to.
		///
		/// @note [Effect Lifetime] Detaching an effect does not destroy it.
		///
		/// @param[in] effect The handle to the effect.
		///
		/// @retval Result::Ok					The effect was successfully detached.
		/// @retval Result::NotInitialized		The engine is not initialized.
		/// @retval Result::InvalidHandle		The effect handle is not recognized.
		/// @retval Result::ExpiredHandle		The effect handle points to an already destroyed effect.
		Result DetachEffect(EffectHandle effect);

		/// @brief Destroys an effect.
		///
		/// If the effect is attached to a bus, the engine will automatically detach it before destroying it.
		///
		/// @note[Attached Effects] Destroying an effect that is attached to a bus that is being played through is
		/// generally not recommended as it can cause a pop in the audio output due to bypassing the built-in effect
		/// fade-out.
		///
		/// @param[in] effect The handle to the effect.
		///
		/// @retval Result::Ok				The effect was successfully destroyed.
		/// @retval Result::NotInitialized	The engine is not initialized.
		/// @retval Result::InvalidHandle	The effect handle is not recognized.
		/// @retval Result::ExpiredHandle	The effect handle points to an already destroyed effect.
		Result DestroyEffect(EffectHandle effect);

#pragma endregion EFFECTS_MANAGEMENT

		// ============================================================================
		// [ PLAYBACK MANAGEMENT ]
		// Methods for creating and modifying playback instances in terms of state
		// and parameters.
		// ============================================================================
#pragma region PLAYBACK_MANAGEMENT

		/// @brief Creates a playback instance with a sound and prepares it for playback.
		///
		/// This function  does not start playback.
		///
		/// @note [Deferred Playback] This function will still succeed if the sound is still being loaded
		/// asynchronously.
		///
		/// @note [Playback Lifecycle] Playback instances are immediately invalidated upon stopping. Because of this,
		/// always check the return value of any playback modifying methods to make sure the handle is still valid.
		///
		/// @param[out] playback	The handle to the playback instance.
		/// @param[in] sound		The handle to the sound.
		/// @param[in] callback		Optional. If provided, this function will be called when the playback terminates.
		///
		/// @retval Result::Ok							The playback instance was successfully created.
		/// @retval Result::NotInitialized				The engine is not initialized.
		/// @retval Result::InvalidHandle				The sound handle is not recognized.
		/// @retval Result::SoundLoadError				The sound asset previously failed to load.
		/// @retval Result::VoicePoolExhausted			The allocated voice capacity has been reached.
		/// @retval Result::StreamPoolExhausted			The allocated stream capacity has been reached.
		/// @retval Result::IoStreamRequestQueueFull	The I/O streaming request queue is at capacity.
		Result CreatePlayback(PlaybackHandle& playback, SoundHandle sound,
		                      PlaybackExitCallback callback = nullptr);

		/// @brief Routes a playback instance to output its audio into the specified mixing bus.
		///
		/// This function can be called both before and during playback.
		///
		/// @param[in] playback			The handle to the playback instance.
		/// @param[in] busIdentifier	The unique string identifier of the target bus.
		///
		/// @retval Result::Ok				The voice was successfully routed.
		/// @retval Result::NotInitialized	The engine is not initialized.
		/// @retval Result::InvalidHandle	The playback handle is not recognized.
		/// @retval Result::ExpiredHandle	The playback handle has already stopped playing.
		/// @retval Result::BusNotFound		The target bus does not exist.
		Result RoutePlayback(PlaybackHandle playback, const char* busIdentifier);

		/// @brief Starts or resumes a playback instance.
		///
		/// If the playback instance is paused, calling this function will resume it from the current position. If
		/// it is inactive, it will play from the start.
		///
		/// @note [Deferred Playback] It is safe to call this function on a playback instance that is still waiting for
		/// its sound to load. The playback will start as soon as the sound has been loaded in.
		///
		/// @param[in] playback The handle to the playback instance.
		///
		/// @retval Result::Ok					The playback instance was successfully set to play.
		/// @retval Result::NotInitialized		The engine is not initialized.
		/// @retval Result::InvalidHandle		The playback handle is not recognized.
		/// @retval Result::ExpiredHandle		The playback handle has already stopped playing.
		/// @retval Result::PlaybackCorrupted	The internal state of the playback instance has been corrupted.
		Result Play(PlaybackHandle playback);

		/// @brief Pauses a playback instance.
		///
		/// @param[in] playback The handle to the playback instance.
		///
		/// @retval Result::Ok					The playback instance was successfully set to pause.
		/// @retval Result::NotInitialized		The engine is not initialized.
		/// @retval Result::InvalidHandle		The playback handle is not recognized.
		/// @retval Result::ExpiredHandle		The playback handle has already stopped playing.
		Result Pause(PlaybackHandle playback);

		/// @brief Stops a playback instance.
		///
		/// If a callback function was provided when creating the playback instance, it will be called with the exit
		/// condition ExplicitStop once the playback has been stopped.
		///
		/// @note [Playback Lifecycle] Successfully calling this function invalidates the provided PlaybackHandle.
		/// A stopped playback cannot be restarted.
		///
		/// @param[in] playback The handle to the playback instance.
		///
		/// @retval Result::Ok					The playback instance was successfully set to stop.
		/// @retval Result::NotInitialized		The engine is not initialized.
		/// @retval Result::InvalidHandle		The playback handle is not recognized.
		/// @retval Result::ExpiredHandle		The playback handle has already stopped playing.
		Result Stop(PlaybackHandle playback);

		/// @brief Seeks a playback instance to a specified time offset.
		///
		/// @param playback			The handle to the playback instance.
		/// @param timeInSeconds	The time offset from the start of the sound (in seconds). This value is
		///							automatically clamped between 0.0 and the length of the sound.
		///
		/// @retval Result::Ok					The seek was successfully performed.
		/// @retval Result::NotInitialized		The engine is not initialized.
		/// @retval Result::InvalidHandle		The playback handle is not recognized.
		/// @retval Result::ExpiredHandle		The playback handle has already stopped playing.
		Result SeekPlayback(PlaybackHandle playback, double timeInSeconds);

		/// @brief Sets the mixing volume of a playback instance in decibels (dB).
		///
		/// @param[in] playback The handle to the playback instance.
		/// @param[in] volumeDb The new volume in decibels (dB). A value of 0.0 represents unity gain. The volume
		///						is automatically clamped to the engine's safe operating range
		///						(between -60.0dB and +12.0dB).
		///
		/// @retval Result::Ok					The mixing volume of the playback instance was successfully set.
		/// @retval Result::NotInitialized		The engine is not initialized.
		/// @retval Result::InvalidHandle		The playback handle is not recognized.
		/// @retval Result::ExpiredHandle		The playback handle has already stopped playing.
		Result SetPlaybackVolumeDb(PlaybackHandle playback, float volumeDb);

		/// @brief Sets the pitch of a playback instance.
		///
		/// @param[in] playback The handle to the playback instance.
		/// @param[in] pitch	The new pitch. This is clamped internally between 0.1 and 4.0 to prevent performance
		///						issues.
		///
		/// @retval Result::Ok					The pitch of the playback instance was successfully set.
		/// @retval Result::NotInitialized		The engine is not initialized.
		/// @retval Result::InvalidHandle		The playback handle is not recognized.
		/// @retval Result::ExpiredHandle		The playback handle has already stopped playing.
		Result SetPlaybackPitch(PlaybackHandle playback, float pitch);

		/// @brief Sets the stereo pan of a playback instance.
		///
		/// @param[in] playback The handle to the playback instance.
		/// @param[in] pan		The normalized pan position. A value of 0.0 represents an equal distribution between
		///						L and R, -1.0 is full L and 1.0 is full R.
		///
		/// @retval Result::Ok					The pan of the playback instance was successfully set.
		/// @retval Result::NotInitialized		The engine is not initialized.
		/// @retval Result::InvalidHandle		The playback handle is not recognized.
		/// @retval Result::ExpiredHandle		The playback handle has already stopped playing.
		Result SetPlaybackStereoPan(PlaybackHandle playback, float pan);

		/// @brief Sets the looping state a playback instance.
		///
		/// This method updates whether a playback instance will automatically restart upon finishing.
		///
		/// @param[in] playback The handle to the playback instance.
		/// @param[in] looping The looping state.
		///
		/// @retval Result::Ok					The playback instance looping state was successfully set.
		/// @retval Result::NotInitialized		The engine is not initialized.
		/// @retval Result::InvalidHandle		The playback handle is not recognized.
		/// @retval Result::ExpiredHandle		The playback handle has already stopped playing.
		Result SetPlaybackLooping(PlaybackHandle playback, bool looping);

		/// @brief Enables or disables 3D spatialization for a playback instance.
		///
		/// @param[in] playback The handle to the playback instance.
		/// @param[in] spatial	True to enable 3D spatialization, false for 2D/global playback.
		///
		/// @retval Result::Ok					The playback spatialization was successfully enabled or disabled.
		/// @retval Result::NotInitialized		The engine is not initialized.
		/// @retval Result::InvalidHandle		The playback handle is not recognized.
		/// @retval Result::ExpiredHandle		The playback handle has already stopped playing.
		Result SetPlaybackSpatial(PlaybackHandle playback, bool spatial);

		/// @brief Defines which listener position is used when calculating the distance attenuation for a playback
		/// instance.
		///
		/// @note[Spatial only] This only affects playback instances that have spatialization enabled.
		///
		/// @param[in] playback The handle to the playback instance.
		/// @param[in] mode		The distance evaluation mode to apply.
		///
		/// @retval Result::Ok					The playback distance mode was successfully applied.
		/// @retval Result::NotInitialized		The engine is not initialized.
		/// @retval Result::InvalidHandle		The playback handle is not recognized.
		/// @retval Result::ExpiredHandle		The playback handle has already stopped playing.
		Result SetPlaybackDistanceMode(PlaybackHandle playback, DistanceMode mode);

		/// @brief Sets the curve used to evaluate volume drop-off over distance for a playback instance.
		///
		/// @note[Spatial only] This only affects playback instances that have spatialization enabled.
		///
		/// @param[in] playback The handle to the playback instance.
		/// @param[in] model	The attenuation curve to apply.
		///
		/// @retval Result::Ok					The playback attenuation curve was successfully set.
		/// @retval Result::NotInitialized		The engine is not initialized.
		/// @retval Result::InvalidHandle		The playback handle is not recognized.
		/// @retval Result::ExpiredHandle		The playback handle has already stopped playing.
		Result SetPlaybackAttenuationCurve(PlaybackHandle playback, AttenuationCurve model);

		/// @brief Updates the 3D world position of a playback instance.
		///
		/// @note[Spatial only] This only affects playback instances that have spatialization enabled.
		///
		/// @param[in] playback The handle to the playback instance.
		/// @param[in] position	The new 3D world coordinates.
		///
		/// @retval Result::Ok					The playback world position was successfully set.
		/// @retval Result::NotInitialized		The engine is not initialized.
		/// @retval Result::InvalidHandle		The playback handle is not recognized.
		/// @retval Result::ExpiredHandle		The playback handle has already stopped playing.
		Result SetPlaybackPosition(PlaybackHandle playback, const Vec3& position);

		/// @brief Defines the distance boundaries for the attenuation curve of a playback instance.
		///
		/// @note[Spatial only] This only affects playback instances that have spatialization enabled.
		///
		/// @note[Safe values] The engine internally ensures that maxDistance is never less than minDistance,
		/// and that minDistance respects the engine's absolute floor limit to prevent division-by-zero.
		///
		/// @param[in] playback		The handle to the playback instance.
		/// @param[in] minDistance	The radius where the volume drop-off begins.
		/// @param[in] maxDistance	The radius where the volume drops to silence.
		///
		/// @retval Result::Ok					The playback min and max distance were successfully set.
		/// @retval Result::NotInitialized		The engine is not initialized.
		/// @retval Result::InvalidHandle		The playback handle is not recognized.
		/// @retval Result::ExpiredHandle		The playback handle has already stopped playing.
		Result SetPlaybackMinMaxDistance(PlaybackHandle playback, float minDistance, float maxDistance);

		/// @brief Enables or disables the Doppler effect for a playback instance.
		///
		/// @note[Spatial only] This only affects playback instances that have spatialization enabled.
		///
		/// @note[Requirements] In order for the doppler pitch shifting to be used, both listener and playback velocity
		///						must be supplied to the engine every frame.
		///
		/// @param[in] playback		The handle to the playback instance.
		/// @param[in] useDoppler	True to enable Doppler pitch shifting, false to disable it.
		///
		/// @retval Result::Ok					The playback Doppler effect was successfully enabled or disabled.
		/// @retval Result::NotInitialized		The engine is not initialized.
		/// @retval Result::InvalidHandle		The playback handle is not recognized.
		/// @retval Result::ExpiredHandle		The playback handle has already stopped playing.
		Result SetPlaybackUseDoppler(PlaybackHandle playback, bool useDoppler);

		/// @brief Sets the scaling factor for the Doppler effect for a playback instance.
		///
		/// @note[Spatial only] This only affects playback instances that have spatialization and Doppler pitch-shifting
		///						enabled.
		///
		/// @param[in] playback			The handle to the playback instance.
		/// @param[in] dopplerFactor	The scaling factor. Clamped internally between 0.0 and 10.0.
		///
		/// @retval Result::Ok					The playback Doppler effect's scaling factor was successfully set.
		/// @retval Result::NotInitialized		The engine is not initialized.
		/// @retval Result::InvalidHandle		The playback handle is not recognized.
		/// @retval Result::ExpiredHandle		The playback handle has already stopped playing.
		Result SetPlaybackDopplerFactor(PlaybackHandle playback, float dopplerFactor);

		/// @brief Sets the velocity of a playback instance.
		///
		/// Velocity is used exclusively to calculate the Doppler effect. If the Effect is disabled, this value is ignored.
		///
		/// @note[Spatial only] This only affects playback instances that have spatialization enabled.
		///
		/// @param[in] playback The handle to the playback instance.
		/// @param[in] velocity	The velocity vector (in m/s).
		///
		/// @retval Result::Ok					The playback velocity was successfully set.
		/// @retval Result::NotInitialized		The engine is not initialized.
		/// @retval Result::InvalidHandle		The playback handle is not recognized.
		/// @retval Result::ExpiredHandle		The playback handle has already stopped playing.
		Result SetPlaybackVelocity(PlaybackHandle playback, const Vec3& velocity);

		/// @brief Sets the listener routing mask for a playback instance.
		///
		/// The routing mask determines which listeners are allowed to hear and evaluate the playback instance.
		///
		/// @note[Spatial only] This only affects playback instances that have spatialization enabled.
		///
		/// @param[in] playback The handle to the playback instance.
		/// @param[in] mask		The listener routing bitmask.
		///
		/// @retval Result::Ok					The listener routing mask for the playback instance was successfully set.
		/// @retval Result::NotInitialized		The engine is not initialized.
		/// @retval Result::InvalidHandle		The playback handle is not recognized.
		/// @retval Result::ExpiredHandle		The playback handle has already stopped playing.
		Result SetPlaybackListenerMask(PlaybackHandle playback, ListenerMask mask);

#pragma endregion PLAYBACK_MANAGEMENT

		// ============================================================================
		// [ LISTENER MANAGEMENT ]
		// Methods for activating, deactivating and modifying listeners.
		// ============================================================================
#pragma region LISTENER_MANAGEMENT

		/// @brief Enables or disables a listener.
		///
		/// @param listenerIndex	The zero-based index of the listener.
		/// @param active			True to enable the listener, false to disable it.
		///
		/// @retval Result::Ok					The listener was successfully enabled or disabled.
		/// @retval Result::NotInitialized		The engine is not initialized.
		/// @retval Result::ListenerNotFound	The listener index exceeds the allocated listener capacity.
		Result SetListenerActive(uint32_t listenerIndex, bool active);

		/// @brief Updates the complete 3D state of a listener.
		///
		/// @param listenerIndex	The zero-based index of the listener.
		/// @param attributes		The new 3D attributes.
		///
		/// @retval Result::Ok					The listener 3D attributes were successfully set.
		/// @retval Result::NotInitialized		The engine is not initialized.
		/// @retval Result::ListenerNotFound	The listener index exceeds the allocated listener capacity.
		Result SetListener3DAttributes(uint32_t listenerIndex, const Listener3DAttributes& attributes);

		/// @brief Updates the world position of a listener.
		///
		/// @param[in] listenerIndex	The zero-based index of the listener.
		/// @param[in] position			The new world position.
		///
		/// @retval Result::Ok					The listener world position was successfully set.
		/// @retval Result::NotInitialized		The engine is not initialized.
		/// @retval Result::ListenerNotFound	The listener index exceeds the allocated listener capacity.
		Result SetListenerPosition(uint32_t listenerIndex, const Vec3& position);

		/// @brief Sets the distance probe world position for a listener.
		///
		/// @note [Usage]	This position is only used for distance attenuation by playback instances with distance mode
		///					set to FromDistanceProbe.
		///
		/// @param[in] listenerIndex			The zero-based index of the listener.
		/// @param[in] distanceProbePosition	The new world position.
		///
		/// @retval Result::Ok					The listener's distance probe world position was successfully set.
		/// @retval Result::NotInitialized		The engine is not initialized.
		/// @retval Result::ListenerNotFound	The listener index exceeds the allocated listener capacity.
		Result SetListenerDistanceProbePosition(uint32_t listenerIndex, const Vec3& distanceProbePosition);

		/// @brief Sets the facing direction of a listener.
		///
		/// @param[in] listenerIndex	The zero-based index of the listener.
		/// @param[in] forward			The forward-facing direction vector. Normalized internally.
		/// @param[in] up				The upward-facing direction vector. Normalized internally.
		///
		/// @retval Result::Ok					The listener orientation was successfully set.
		/// @retval Result::NotInitialized		The engine is not initialized.
		/// @retval Result::ListenerNotFound	The listener index exceeds the allocated listener capacity.
		Result SetListenerOrientation(uint32_t listenerIndex, const Vec3& forward, const Vec3& up);

		/// @brief Sets the velocity of a listener.
		///
		/// @param[in] listenerIndex	The zero-based index of the listener.
		/// @param[in] velocity			The velocity vector (in m/s).
		///
		/// @retval Result::Ok					The listener velocity was successfully set.
		/// @retval Result::NotInitialized		The engine is not initialized.
		/// @retval Result::ListenerNotFound	The listener index exceeds the allocated listener capacity.
		Result SetListenerVelocity(uint32_t listenerIndex, const Vec3& velocity);

#pragma endregion LISTENER_MANAGEMENT

	private:
		void TeardownInternal();

		EngineInternalState* m_state = nullptr;

	};
}