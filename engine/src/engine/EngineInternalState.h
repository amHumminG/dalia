#pragma once

#include "dalia/Engine.h"

// --- NEW ---
#include "async/messaging/AsyncMessageBus.h"

#include "backend/DeviceController.h"
#include "asset/AssetController.h"
#include "io/IoController.h"
#include "mixer/MixController.h"

#include "async/WakeableThread.h"
#include "async/IntervalThread.h"
// ------


#include "core/Types.h"
#include "core/StateSyncPool.h"
#include "core/AsyncPool.h"

#include "mixer/Voice.h"
#include "mixer/Listener.h"
#include "mixer/Bus.h"
#include "mixer/effects/Biquad.h"
#include "mixer/effects/EffectTraits.h"

#include <memory>

namespace dalia {

	class AudioBackend;
	class OutputDevice;
	class NullOutputDevice;
	class MixerController;
	class AsyncStreamSystem;
	class AsyncLoadSystem;
	class AsyncControlSystem;
	class AssetRegistry;
	class MixGraphCompiler;

	struct StreamContext;

	/// @brief The internal implementation state of the DALIA engine.
	struct EngineInternalState {
		bool initialized = false;

		// Engine Settings
		CoordinateSystem coordinateSystem = CoordinateSystem::LeftHanded;

		std::unique_ptr<AsyncMessageBus>	asyncMessageBus;

		std::unique_ptr<DeviceController>	deviceController;
		std::unique_ptr<AssetController>	assetController;
		std::unique_ptr<IoController>		ioController;
		// std::unique_ptr<MixController>	mixController;
		// std::unique_ptr<Mixer>			mixer;

		// Threads
		std::unique_ptr<IntervalThread> streamThread;
		std::unique_ptr<WakeableThread> loadThread;
		std::unique_ptr<WakeableThread> controlThread;

		// Mixer Nodes
		StateSyncPool<Voice, VoiceMirror, VoiceParams> voices;				// TODO: Move to mixController
		StateSyncPool<Listener, ListenerMirror, ListenerParams> listeners;	// TODO: Move to mixController
		StateSyncPool<Biquad, BiquadMirror, BiquadParams> biquads;			// TODO: Move to mixController
		StateSyncPool<Bus, BusMirror, BusParams> buses;						// TODO: Move to mixController
		std::unordered_map<BusID, uint32_t>		 busMap;					// TODO: Move to mixController
		std::unique_ptr<float[]>				 busBufferPool;				// TODO: Move to mixController

		// DSP Utility
		std::unique_ptr<MixGraphCompiler> mixGraphCompiler;	// TODO: Move to mixer
		std::unique_ptr<uint32_t[]> mixOrder;				// TODO: Move to mixer
		std::unique_ptr<float[]> mixScratchBuffer;			// TODO: Move to mixer

		// Systems
		std::unique_ptr<MixerController>	asyncMixerSystem;	// TODO: Rework

		EngineInternalState(const EngineConfig& config);

		/// @brief Engine helper to check if internal state is initialized.
		/// @param state The engine internal state.
		/// @return True if initialized, otherwise false.
		static bool IsInitialized(const EngineInternalState* state);

		/// @brief Generates a new load request identifier.
		/// @return The new request identifier.
		uint32_t GenerateIoLoadRequestId();

		/// @brief Creates a playback handle based on a voice index and generation.
		/// @param index The index of the voice.
		/// @param gen The generation of the voice.
		/// @return The constructed handle.
		static PlaybackHandle ForgePlaybackHandle(uint32_t index, uint32_t gen);

		/// @brief Creates a sound handle based on a raw 64-bit identifier.
		/// @param rawId The raw 64-bit identifier.
		/// @return The constructed handle.
		static SoundHandle ForgeSoundHandle(uint64_t rawId);

		/// @brief Creates an effect handle based on a raw 64-bit identifier.
		/// @param rawId The raw 64-bit identifier.
		/// @return The constructed handle.
		static EffectHandle ForgeEffectHandle(uint64_t rawId);

		// TODO: Write doxygen comments for these functions
		static uint32_t GetPlaybackIndex(PlaybackHandle playback);
		static uint32_t GetPlaybackGeneration(PlaybackHandle playback);
		static uint32_t GetEffectIndex(EffectHandle effect);
		static uint32_t GetEffectGen(EffectHandle effect);



		/// @brief Validates a playback handle.
		/// @param playback The handle to the playback instance.
		/// @return Dalia result code. Result::Ok if valid.
		Result Validate(PlaybackHandle playback);

		/// @brief Validates a listener index.
		/// @param listenerIndex The index of the listener instance.
		/// @return Dalia result code. Result::Ok if valid.
		Result Validate(uint32_t listenerIndex);

		/// @brief Validates a bus identifier.
		/// @param busIdentifier The identifier of the bus instance.
		/// @return Dalia result code. Result::Ok if valid.
		Result Validate(const char* busIdentifier);

		/// @brief Validates a effect handle.
		/// @param effect The handle to the effect instance.
		/// @return Dalia result code. Result::Ok if valid.
		Result Validate(EffectHandle effect);

		/// @brief Frees an effect from its pool. This function performs a bounds check but it doesn't perform any
		/// generation check.
		/// @param effect The handle to the effect instance.
		/// @return Dalia result code. Result::Ok if the effect was successfully freed.
		Result FreeEffectFromPool(EffectHandle effect);

		/// @brief Detaches an effect from its bus.
		/// @param effect The handle to the effect instance.
		/// @param fadeOut Determines whether or not the effect should fade out before getting detached by the mixer.
		/// @return Dalia result code. Result::Ok if the effect was successfully detached or if it was already detached.
		Result DetachEffectFromBus(EffectHandle effect, bool fadeOut);
	};
}
