#pragma once

#include "dalia/Engine.h"
#include "dalia/Result.h"
#include "dalia/PlaybackControl.h"
#include "dalia/SoundControl.h"
#include "dalia/EffectControl.h"

#include "core/Types.h"
#include "core/StateSyncPool.h"

#include "mixer/Voice.h"
#include "mixer/Listener.h"
#include "mixer/Bus.h"
#include "mixer/effects/EffectTraits.h"
#include "mixer/effects/Biquad.h"

#include <unordered_map>
#include <vector>
#include <utility>

namespace dalia {

	class AsyncMessageBus;
	class AssetController;
	class IoController;
	struct AsyncMixerEvent;

	/// @brief Represents a playback instance waiting for an asset to be loaded before it can start playing.
	struct PendingPlayback {
		PlaybackHandle playback;
		uint64_t assetRawId = INVALID_RAW_ID;
	};

	struct MixControllerConfig {
		CoordinateSystem coordinateSystem = CoordinateSystem::LeftHanded;
		uint32_t voiceCapacity = 64;
		uint32_t listenerCapacity = 1;
		uint32_t busCapacity = 32;
		uint32_t biquadCapacity = 16;
	};

	class MixController {
	public:
		explicit MixController(const EngineConfig& config); // TODO: Change params
		~MixController();

		Result Initialize(
			AsyncMessageBus* asyncMessageBus,
			AssetController* assetController,
			IoController* ioController,
			CoordinateSystem coordinateSystem
		);
		void Shutdown();

		/// @brief Syncs dirty mirror parameters with their mixer counterparts.
		void SyncPools();

		void OnSoundLoadComplete(SoundHandle sound, Result loadResult);
		uint32_t AbortAndStopVoicesForSound(SoundHandle sound);

		// === Playback API ===
		Result CreatePlayback(
			PlaybackHandle& playback,
			SoundHandle sound,
			const char* bus,
			PlaybackExitCallback exitCallback
		);

		Result PlayPlayback(PlaybackHandle playback, uint32_t fadeInMs);
		Result PausePlayback(PlaybackHandle playback, uint32_t fadeOutMs);
		Result StopPlayback(PlaybackHandle playback, uint32_t fadeOutMs);
		Result SeekPlayback(PlaybackHandle playback, double timeInSeconds);

		Result SetPlaybackVolumeDb(PlaybackHandle playback, float volumeDb);
		Result SetPlaybackRate(PlaybackHandle playback, float rate);
		Result SetPlaybackStereoPan(PlaybackHandle playback, float stereoPan);
		Result SetPlaybackLooping(PlaybackHandle playback, bool looping);

		Result RoutePlayback(PlaybackHandle playback, const char* bus);

		Result SetPlaybackSpatial(PlaybackHandle playback, bool spatial);
		Result SetPlaybackDistanceMode(PlaybackHandle playback, DistanceMode distanceMode);
		Result SetPlaybackAttenuationCurve(PlaybackHandle playback, AttenuationCurve curve);
		Result SetPlaybackMinMaxDistance(PlaybackHandle playback, float minDistance, float maxDistance);
		Result SetPlaybackPosition(PlaybackHandle playback, Vec3 position);
		Result SetPlaybackVelocity(PlaybackHandle playback, Vec3 velocity);
		Result SetPlaybackUseDoppler(PlaybackHandle playback, bool useDoppler);
		Result SetPlaybackDopplerFactor(PlaybackHandle playback, float dopplerFactor);
		Result SetPlaybackListenerMask(PlaybackHandle playback, ListenerMask listenerMask);

		// === Bus API ===


		// === Listener API ===


		// === Effect API ===


		// === DSP Pool Accessors ===
		[[nodiscard]] auto& GetVoicePool() { return m_voices; }
		[[nodiscard]] auto& GetListenerPool() { return m_listeners; }
		[[nodiscard]] auto& GetBusPool() { return m_buses; }
		[[nodiscard]] auto& GetBiquadPool() { return m_biquads; }

	private:
		template<typename TParams>
		friend struct EffectTraits;

		// --- Lifecycle Helpers ---
		void HandleVoiceStoppedEvent(const AsyncMixerEvent& ev);
		Result PrepareDeferredVoice(PlaybackHandle playback, SoundHandle sound);
		void ResetAndFreeVoice(uint32_t vIndex);

		// --- Internal Validation & Lookup ---

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

		/// @brief Retrieves the index of a bus instance based on its identifier.
		/// @param busIdentifier The identifier of the bus instance.
		/// @param index The index to be populated.
		/// @return Dalia result code. Result::InvalidBus if the identifier does not match a valid bus instance.
		Result GetBusIndex(const char* busIdentifier, uint32_t& index);

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

		bool WouldCreateBusCycle(uint32_t busIndex, uint32_t candidateParentBusIndex) const;

		/// @brief Primitive for modifying the state of a voice. Handles all validation checks.
		/// @tparam TFunc The type of the callable task.
		/// @param playback The handle to the playback instance representing the voice.
		/// @param fn Modification task taking (VoiceMirror& vMirror, uint32_t vIndex). Should return a dalia::Result.
		/// @return Dalia result code.
		template<typename TFunc>
		Result ModifyVoice(PlaybackHandle playback, TFunc&& fn) {
			if (!playback.IsValid()) return Result::InvalidHandle;

			uint32_t vIndex = playback.GetIndex();
			if (vIndex >= m_voices.Capacity()) return Result::InvalidHandle;

			VoiceMirror& vMirror = m_voices.GetMirror(vIndex);
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
				m_voices.MarkDirty(vIndex);
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
			if (listenerIndex >= m_listeners.Capacity()) return Result::InvalidListener;

			return fn(m_listeners.GetMirror(listenerIndex), listenerIndex);
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
				m_listeners.MarkDirty(lIndex);
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
			auto it = m_busMap.find(bId);
			if (it == m_busMap.end()) return Result::InvalidBus;

			return fn(m_buses.GetMirror(it->second), it->second);
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
				m_buses.MarkDirty(bIndex);
				return Result::Ok;
			});
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

		/// @brief Runtime dispatcher for effect modification. Handles all validation checks.
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

		CoordinateSystem m_coordinateSystem = CoordinateSystem::LeftHanded;

		StateSyncPool<Voice, VoiceMirror, VoiceParams>			m_voices;
		StateSyncPool<Listener, ListenerMirror, ListenerParams> m_listeners;
		StateSyncPool<Bus, BusMirror, BusParams>				m_buses;
		std::unordered_map<BusID, uint32_t>						m_busMap;
		StateSyncPool<Biquad, BiquadMirror, BiquadParams>		m_biquads;

		std::vector<PendingPlayback> m_pendingPlaybacks;

		AsyncMessageBus* m_asyncMessageBus = nullptr;
		AssetController* m_assetController = nullptr; // Should this really be a member variable?
		IoController*	 m_ioController	   = nullptr; // Should this really be a member variable?
	};

	inline auto& EffectTraits<BiquadParams>::GetPool(MixController& controller) {
		return controller.GetBiquadPool();
	}
}
