#include "mixer/MixController.h"

#include "asset/AssetController.h"
#include "backend/DeviceController.h"
#include "io/IoController.h"

namespace dalia {

	MixController::MixController(const MixControllerConfig& config)
		: m_coordinateSystem(config.coordinateSystem),
		m_voices(config.voiceCapacity),
		m_listeners(config.listenerCapacity),
		m_buses(config.busCapacity),
		m_biquads(config.biquadCapacity) {

		m_busMap.reserve(config.busCapacity);
	}

	MixController::~MixController() {
		Shutdown();
	}

	Result MixController::Initialize(AsyncMessageBus* asyncMessageBus, AssetController* assetController,
		IoController* ioController) {

		m_asyncMessageBus = asyncMessageBus;
		m_assetController = assetController;
		m_ioController	  = ioController;

		// Initialize master bus at index 0
		uint32_t masterIndex; // Temporary
		m_buses.Allocate(masterIndex);
		assert(masterIndex == MASTER_BUS_INDEX && "Master bus was not allocated at index 0.");
		m_buses.GetMirror(MASTER_BUS_INDEX).refCount = 1;
		m_buses.Get(MASTER_BUS_INDEX).isActive = true;
		constexpr BusID masterId("Master");
		m_busMap[masterId] = MASTER_BUS_INDEX;

		// Subscribe to mixer events
		m_asyncMessageBus->SubscribeToMixerEvent(AsyncMixerEvent::Type::VoiceStopped,
			[this](const AsyncMixerEvent& ev) { HandleVoiceStoppedEvent(ev); });

		// Subscribe to load events
		m_asyncMessageBus->SubscribeToLoadEvent(AsyncLoadEvent::Type::SoundLoaded,
			[this](const AsyncLoadEvent& ev) { HandleSoundLoadedEvent(ev); });
		m_asyncMessageBus->SubscribeToLoadEvent(AsyncLoadEvent::Type::SoundLoadFailed,
			[this](const AsyncLoadEvent& ev) { HandleSoundLoadFailedEvent(ev); });

		return Result::Ok;
	}

	void MixController::Shutdown() {
		if (!m_asyncMessageBus) return;

		// Unsubscribe from mixer events
		m_asyncMessageBus->UnsubscribeFromMixerEvent(AsyncMixerEvent::Type::VoiceStopped);

		// Unsubscribe from load events
		m_asyncMessageBus->UnsubscribeFromLoadEvent(AsyncLoadEvent::Type::SoundLoaded);
		m_asyncMessageBus->UnsubscribeFromLoadEvent(AsyncLoadEvent::Type::SoundLoadFailed);

		m_asyncMessageBus = nullptr;
		m_assetController = nullptr;
		m_ioController	  = nullptr;
	}

	void MixController::SyncPools() {
		m_voices.Sync();
		m_listeners.Sync();
		m_buses.Sync();
		m_biquads.Sync();
	}

	Result MixController::Validate(PlaybackHandle playback) {
		return ModifyVoice(playback, [](const VoiceMirror&, uint32_t) { return Result::Ok; });
	}

	Result MixController::Validate(uint32_t listenerIndex) {
		return ModifyListener(listenerIndex, [](const ListenerMirror&, uint32_t) { return Result::Ok; });
	}

	Result MixController::Validate(const char* busIdentifier) {
		return ModifyBus(busIdentifier, [](const BusMirror&, uint32_t) { return Result::Ok; });
	}

	Result MixController::Validate(EffectHandle effect) {
		return ModifyEffect(effect, [](const EffectMirror&, uint32_t) { return Result::Ok; });
	}

	Result MixController::GetBusIndex(const char* busIdentifier, uint32_t& index) {
		if (!busIdentifier) return Result::InvalidBus;

		const BusID bId(busIdentifier);
		auto it = m_busMap.find(bId);
		if (it == m_busMap.end()) return Result::InvalidBus;

		index = it->second;
		return Result::Ok;
	}

	Result MixController::FreeEffectFromPool(EffectHandle effect) {
		uint32_t eIndex = effect.GetIndex();
		switch (effect.GetType()) {
			case EffectType::Biquad:
				if (eIndex >= m_biquads.Capacity()) return Result::InvalidHandle;
				m_biquads.Free(eIndex);
				return Result::Ok;
			default:
				return Result::InvalidHandle;
		}
	}

	Result MixController::DetachEffectFromBus(EffectHandle effect, bool fadeOut) {
		return ModifyEffect(effect, [&](EffectMirror& eMirror, uint32_t) -> Result {
			if (!eMirror.IsAttached()) return Result::Ok;

			uint32_t bIndex = eMirror.busIndex;
			uint32_t slot = eMirror.effectSlot;

			BusMirror& bMirror = m_buses.GetMirror(bIndex);
			if (bMirror.effectSlots[slot] == effect) bMirror.effectSlots[slot] = INVALID_EFFECT_HANDLE;

			eMirror.busIndex = NO_INDEX;
			eMirror.effectSlot = NO_INDEX;

			if (fadeOut) {
				m_asyncMessageBus->PushMixerCommand(AsyncMixerCommand::FadeDetachEffect(
					effect.GetIndex(), eMirror.gen, effect.GetType(), bIndex, slot
				));
			}
			else {
				m_asyncMessageBus->PushMixerCommand(AsyncMixerCommand::ForceDetachEffect(
					effect.GetIndex(), eMirror.gen, effect.GetType(), bIndex, slot
				));
			}

			return Result::Ok;
		});
	}

	bool MixController::WouldCreateBusCycle(uint32_t busIndex, uint32_t candidateParentBusIndex) const {
		uint32_t currentAncestor = candidateParentBusIndex;
		while (currentAncestor != NO_PARENT) {
			if (currentAncestor == busIndex) return true;
			currentAncestor = m_buses.GetMirror(currentAncestor).parentBusIndex;
		}
		return false;
	}

	void MixController::HandleVoiceStoppedEvent(const AsyncMixerEvent& ev) {
		uint32_t vIndex = ev.data.voice.index;
		uint32_t vGen = ev.data.voice.gen;
		if (vIndex >= m_voices.Capacity()) return;

		VoiceMirror& vMirror = m_voices.GetMirror(vIndex);
		if (vMirror.gen != vGen) return;

		PlaybackExitCallback exitCallback = std::move(vMirror.onStopCallback);

		ResetAndFreeVoice(vIndex);

		// Invoke registered callback
		exitCallback(PlaybackHandle::Create(vIndex, vGen), ev.data.voice.exitCondition);
	}

	void MixController::HandleSoundLoadedEvent(const AsyncLoadEvent& ev) {
		SoundHandle sound = SoundHandle::FromRawId(ev.assetRawId);

		// Notify asset controller
		m_assetController->FinalizeAsyncSoundLoad(sound, ev.formatInfo);

		// Wake up deferred playbacks waiting on the sound
		for (auto it = m_pendingPlaybacks.begin(); it != m_pendingPlaybacks.end();) {
			if (it->assetRawId != ev.assetRawId) {
				++it;
				continue;
			}

			PlaybackHandle pb = it->playback;
			it = m_pendingPlaybacks.erase(it);

			uint32_t vIndex = pb.GetIndex();
			if (m_voices.GetMirror(vIndex).gen != pb.GetGeneration()) continue;

			if (PrepareDeferredVoice(pb, sound) != Result::Ok) ResetAndFreeVoice(vIndex);
		}

		// Invoke registered load callback
		m_assetController->InvokeLoadCallback(ev.requestId, ev.result);
	}

	void MixController::HandleSoundLoadFailedEvent(const AsyncLoadEvent& ev) {
		SoundHandle sound = SoundHandle::FromRawId(ev.assetRawId);

		// Notify asset controller
		m_assetController->FailSoundLoad(sound);

		// Abort deferred playbacks waiting on the sound
		for (auto it = m_pendingPlaybacks.begin(); it != m_pendingPlaybacks.end();) {
			if (it->assetRawId != ev.assetRawId) {
				++it;
				continue;
			}

			PlaybackHandle pb = it->playback;
			it = m_pendingPlaybacks.erase(it);

			uint32_t vIndex = pb.GetIndex();
			if (m_voices.GetMirror(vIndex).gen != pb.GetGeneration()) ResetAndFreeVoice(vIndex);
		}

		// Invoke registered load callback
		m_assetController->InvokeLoadCallback(ev.requestId, ev.result);
	}

	Result MixController::PrepareDeferredVoice(PlaybackHandle playback, SoundHandle sound) {
		uint32_t vIndex = playback.GetIndex();
		VoiceMirror& vMirror = m_voices.GetMirror(vIndex);

		if (sound.GetType() == SoundType::Resident) {
			ResidentSound* rs = m_assetController->GetResidentSound(sound);
			if (!rs || rs->state != LoadState::Loaded) return Result::Error; // TODO: Better error

			vMirror.totalFrames = rs->totalFrames;
			vMirror.sampleRate  = rs->sampleRate;

			m_asyncMessageBus->PushMixerCommand(AsyncMixerCommand::PrepareVoiceResident(
				vIndex, vMirror.gen, rs->pcmData.data(),
				rs->totalFrames, rs->channels, rs->sampleRate
			));
		}
		else if (sound.GetType() == SoundType::Stream) {
			StreamSound* ss = m_assetController->GetStreamSound(sound);
			if (!ss || ss->state != LoadState::Loaded) return Result::Error; // TODO: Better error

			uint32_t sIndex = NO_INDEX;
			uint32_t sGen = NO_GENERATION;
			Result streamRes = m_ioController->AllocateAndPrepareStream(ss->filepath, sIndex, sGen);
			if (streamRes != Result::Ok) return streamRes;

			vMirror.totalFrames = ss->totalFrames;
			vMirror.sampleRate  = ss->sampleRate;

			m_asyncMessageBus->PushMixerCommand(AsyncMixerCommand::PrepareVoiceStreaming(
				vIndex, vMirror.gen, sIndex, sGen,
				ss->channels, ss->sampleRate
			));
		}

		if (vMirror.initialSeekTarget > 0.0) {
			auto targetFrame = static_cast<uint32_t>(vMirror.initialSeekTarget * vMirror.sampleRate);
			if (targetFrame < vMirror.totalFrames) {
				m_asyncMessageBus->PushMixerCommand(AsyncMixerCommand::SeekVoice(
					vIndex, vMirror.gen, targetFrame
				));
			}
		}

		if (vMirror.state == VoiceState::Playing) {
			m_asyncMessageBus->PushMixerCommand(AsyncMixerCommand::PlayVoice(
				vIndex, vMirror.gen
			));
		}

		return Result::Ok;
	}

	void MixController::ResetAndFreeVoice(uint32_t vIndex) {
		VoiceMirror& vMirror = m_voices.GetMirror(vIndex);
		uint64_t assetRawId = vMirror.assetRawId;

		vMirror.Reset();
		m_voices.Free(vIndex);

		if (m_assetController && assetRawId != INVALID_RAW_ID) {
			m_assetController->ReleaseSound(SoundHandle::FromRawId(assetRawId));
		}
	}
}
