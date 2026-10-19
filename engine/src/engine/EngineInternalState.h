#pragma once

#include "dalia/Engine.h"

#include "dalia/Engine.h"

#include "core/Types.h"
#include "core/StateSyncPool.h"
#include "core/AsyncPool.h"

#include "messaging/AsyncControlMessaging.h"
#include "messaging/AsyncLoadMessaging.h"
#include "messaging/AsyncStreamMessaging.h"
#include "messaging/MixerMessaging.h"

#include "mixer/Voice.h"
#include "mixer/Listener.h"
#include "mixer/Bus.h"
#include "mixer/effects/Biquad.h"
#include "mixer/effects/EffectTraits.h"

#include "mixer/Speakers.h"

#include <memory>

namespace dalia {

	class DeviceManager;
	class OutputDevice;
	class NullOutputDevice;
	class MixerSystem;
	class AsyncStreamSystem;
	class AsyncLoadSystem;
	class AsyncControlSystem;
	class AssetRegistry;
	class MixGraphCompiler;

	struct StreamContext;

	struct PendingSoundUnload {
		SoundHandle handle;
		std::vector<PlaybackHandle> voicesToStop;
	};

	struct PendingPlayback {
		PlaybackHandle playback;
		uint64_t assetRawId = INVALID_RAW_ID;
	};

	struct EngineInternalState {
		bool initialized = false;

		// Devices
		std::unique_ptr<DeviceManager> deviceManager;
		std::unique_ptr<OutputDevice> outputDevice;
		std::unique_ptr<NullOutputDevice> nullOutputDevice;
		OutputDevice* activeOutputDevice; // Points to either outputDevice or nullDevice

		std::vector<OutputDeviceInfo> cachedOutputDeviceList;
		std::string targetOutputDeviceId = "default";
		bool activeOutputDeviceIsDefault = true;
		bool pendingOutputDeviceSwap = false;
		bool isSwappingOutputDevice = false;

		SpeakerLayout speakerLayout;
		uint32_t maxSamplesPerPeriod = 0;
		uint32_t outChannels = 0;
		uint32_t outSampleRate = 0;

		// Engine Settings
		CoordinateSystem coordinateSystem;

		// Messaging Queues
		std::unique_ptr<MixerCommandQueue>			mixerCommands;
		std::unique_ptr<MixerEventQueue>			mixerEvents;
		std::unique_ptr<AsyncStreamRequestQueue>	asyncStreamRequests;
		std::unique_ptr<AsyncLoadRequestQueue>		asyncLoadRequests;
		std::unique_ptr<AsyncLoadEventQueue>		asyncLoadEvents;
		std::unique_ptr<AsyncControlRequestQueue>	asyncControlRequests;
		std::unique_ptr<AsyncControlEventQueue>		asyncControlEvents;

		// Streams
		AsyncPool<StreamContext> streams;

		// Voices
		StateSyncPool<Voice, VoiceMirror, VoiceParams> voices;

		// Listeners
		StateSyncPool<Listener, ListenerMirror, ListenerParams> listeners;

		// Buses
		StateSyncPool<Bus, BusMirror, BusParams> buses;
		std::unordered_map<BusID, uint32_t>		busMap;
#if DALIA_DEBUG
		std::unordered_map<BusID, std::string>	busDebugNameMap; // Unused
#endif
		std::unique_ptr<float[]>				busBufferPool;

		// Effects
		StateSyncPool<Biquad, BiquadMirror, BiquadParams> biquads;

		// Mixing & DSP
		std::unique_ptr<MixGraphCompiler> mixGraphCompiler;
		std::unique_ptr<uint32_t[]> mixOrder;

		std::unique_ptr<float[]> mixScratchBuffer;

		// Assets
		std::unique_ptr<AssetRegistry> assetRegistry;
		uint32_t nextIoLoadRequestId = 1;
		std::unordered_map<uint32_t, AssetLoadCallback> loadCallbacks;

		// Deferred Unloads & Playbacks
		std::vector<PendingSoundUnload> pendingSoundUnloads;
		std::vector<PendingPlayback> pendingPlaybacks;

		// Systems
		std::unique_ptr<MixerSystem> mixerSystem;
		std::unique_ptr<AsyncStreamSystem> asyncStreamSystem;
		std::unique_ptr<AsyncLoadSystem> asyncLoadSystem;
		std::unique_ptr<AsyncControlSystem> asyncControlSystem;

		EngineInternalState(const EngineConfig& config);

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

		/// @brief Primitive for modifying the state of a voice. Handles all validation checks.
		/// @tparam TFunc The type of the callable task.
		/// @param playback The handle to the playback instance representing the voice.
		/// @param fn Modification task taking (VoiceMirror& vMirror, uint32_t vIndex). Should return a dalia::Result.
		/// @return Dalia result code.
		template<typename TFunc>
		Result ModifyVoice(PlaybackHandle playback, TFunc&& fn) {
			if (!playback.IsValid()) return Result::InvalidHandle;

			uint32_t vIndex = playback.GetIndex();
			if (vIndex >= voices.Capacity()) return Result::InvalidHandle;

			VoiceMirror& vMirror = voices.GetMirror(vIndex);
			if (vMirror.gen != playback.GetGeneration()) return Result::ExpiredHandle;

			return fn(vMirror, vIndex);
		}

		/// @brief Convenience wrapper for modifying the DSP parameters of a voice. Automatically flags the parameters
		/// as dirty.
		/// @tparam TFunc The type of the callable task.
		/// @param playback The handle to the playback instance representing the voice.
		/// @param fn Modification task taking (VoiceParams& params).
		/// @return Dalia result code.
		template<typename TFunc>
		Result ModifyVoiceParams(PlaybackHandle playback, TFunc&& fn) {
			return ModifyVoice(playback, [&](VoiceMirror& vMirror, uint32_t vIndex) -> Result {
				fn(vMirror.params);
				voices.MarkDirty(vIndex);
				return Result::Ok;
			});
		}

		/// @brief Primitive for modifying the state of a listener. Handles all validation checks.
		/// @tparam TFunc The type of the callable task.
		/// @param listenerIndex The index of the listener instance.
		/// @param fn Modification task taking (ListenerMirror& lMirror, uint32_t lIndex). Should return a dalia::Result.
		/// @return Dalia result code.
		template<typename TFunc>
		Result ModifyListener(uint32_t listenerIndex, TFunc&& fn) {
			if (listenerIndex >= listeners.Capacity()) return Result::InvalidListener;

			return fn(listeners.GetMirror(listenerIndex), listenerIndex);
		}

		/// @brief Convenience wrapper for modifying the DSP parameters of a listener. Automatically flags the parameters
		/// as dirty.
		/// @tparam TFunc The type of the callable task.
		/// @param listenerIndex The index of the listener instance.
		/// @param fn Modification task taking (ListenerParams& params).
		/// @return Dalia result code.
		template<typename TFunc>
		Result ModifyListenerParams(uint32_t listenerIndex, TFunc&& fn) {
			return ModifyListener(listenerIndex, [&](ListenerMirror& lMirror, uint32_t lIndex) -> Result {
				fn(lMirror.params);
				listeners.MarkDirty(lIndex);
				return Result::Ok;
			});
		}

		/// @brief Primitive for modifying the state of a bus. Handles all validation checks.
		/// @tparam TFunc The type of the callable task.
		/// @param identifier The identifier of the bus instance.
		/// @param fn Modification task taking (BusMirror& bMirror, uint32_t bIndex). Should return a dalia::Result.
		/// @return Dalia result code.
		template<typename TFunc>
		Result ModifyBus(const char* identifier, TFunc&& fn) {
			if (!identifier) return Result::InvalidBus;

			const BusID bId{identifier};
			auto it = busMap.find(bId);
			if (it == busMap.end()) return Result::InvalidBus;

			return fn(buses.GetMirror(it->second), it->second);
		}

		/// @brief Convenience wrapper for modifying the DSP parameters of a bus. Automatically flags the parameters
		/// as dirty.
		/// @tparam TFunc The type of the callable task.
		/// @param identifier The identifier of the bus instance.
		/// @param fn Modification task taking (BusParams& params).
		/// @return Dalia result code.
		template<typename TFunc>
		Result ModifyBusParams(const char* identifier, TFunc&& fn) {
			return ModifyBus(identifier, [&](BusMirror& bMirror, uint32_t bIndex) -> Result {
				fn(bMirror.params);
				buses.MarkDirty(bIndex);
				return Result::Ok;
			});
		}

		/// @brief Retrieves the index of a bus instance based on its identifier.
		/// @param busIdentifier The identifier of the bus instance.
		/// @param index The index to be populated.
		/// @return Dalia result code. Result::InvalidBus if the identifier does not match a valid bus instance.
		Result GetBusIndex(const char* busIdentifier, uint32_t& index) {
			if (!busIdentifier) return Result::InvalidBus;

			const BusID bId(busIdentifier);
			auto it = busMap.find(bId);
			if (it == busMap.end()) return Result::InvalidBus;

			index = it->second;
			return Result::Ok;
		}

		/// @brief Primitive for modifying the state of an effect. Handles all validation checks.
		/// @tparam TParams The parameter struct defining the effect type.
		/// @tparam TFunc The type of the callable task.
		/// @param effect The handle to the effect instance.
		/// @param fn Modification task taking (EffectMirror& eMirror, uint32_t eIndex). Should return a dalia::Result.
		/// @return Dalia result code.
		template<typename TParams, typename TFunc>
		Result ModifyEffect(EffectHandle effect, TFunc&& fn) {
			using Traits = EffectTraits<TParams>;
			if (!effect.IsValid() || effect.GetType() != Traits::TYPE) return Result::InvalidHandle;

			auto& ePool = Traits::GetPool(*this);
			uint32_t eIndex = effect.GetIndex();
			if (eIndex >= ePool.Capacity()) return Result::InvalidHandle;

			auto& eMirror = ePool.GetMirror(eIndex);
			if (eMirror.gen != effect.GetGeneration()) return Result::ExpiredHandle;

			return fn(eMirror, eIndex);
		}

		/// @brief Runtime dispatcher for effect modification.
		/// @tparam TFunc The parameter struct defining the effect type.
		/// @param effect The handle to the effect instance.
		/// @param fn Modification task taking (auto& eMirror, uint32_t eIndex). Should return a dalia::Result.
		/// @return Dalia result code.
		template<typename TFunc>
		Result ModifyEffect(EffectHandle effect, TFunc&& fn) {
			switch (effect.GetType()) {
				case EffectType::Biquad:
					return ModifyEffect<BiquadParams>(effect, std::forward<TFunc>(fn));
				default:
					return Result::InvalidHandle;
			}
		}

		/// @brief Convenience wrapper for modifying the DSP parameters of an effect. Automatically flags the parameters
		/// as dirty.
		/// @tparam TParams The parameter struct defining the effect type.
		/// @tparam TFunc The type of the callable task.
		/// @param effect The handle to the effect instance.
		/// @param fn Modification task taking (TParams& params).
		/// @return Dalia result code.
		template<typename TParams, typename TFunc>
		Result ModifyEffectParams(EffectHandle effect, TFunc&& fn) {
			return ModifyEffect<TParams>(effect, [&](auto& eMirror, uint32_t eIndex) -> Result {
				fn(eMirror.params);
				EffectTraits<TParams>::GetPool(*this).MarkDirty(eIndex);
				return Result::Ok;
			});
		}

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
