#include "mixer/MixController.h"

#include "../../../sandbox/src/PlaybackInstance.h"

namespace dalia {
	MixController::MixController(const EngineConfig& config)
		: m_voices(config.voiceCapacity),
		m_listeners(config.listenerCapacity),
		m_buses(config.busCapacity),
		m_biquads(config.biquadCapacity) {
		m_busMap.reserve(config.busCapacity);
	}

	MixController::~MixController() {
		Shutdown();
	}

	Result MixController::Initialize(AsyncMessageBus* asyncMessageBus, AssetController* assetController,
		IoController* ioController, CoordinateSystem coordinateSystem) {

		m_asyncMessageBus = asyncMessageBus;
		m_assetController = assetController;
		m_ioController = ioController;
		m_coordinateSystem = coordinateSystem;

		// Initialize master bus at index 0
		uint32_t masterIndex; // Temporary
		m_buses.Allocate(masterIndex);
		assert(masterIndex == MASTER_BUS_INDEX && "Master bus was not allocated at index 0.");
		m_buses.GetMirror(MASTER_BUS_INDEX).refCount = 1;
		m_buses.Get(MASTER_BUS_INDEX).isActive = true;
		constexpr BusID masterId("Master");
		m_busMap[masterId] = MASTER_BUS_INDEX;

		m_asyncMessageBus->SubscribeToMixerEvent(AsyncMixerEvent::Type::VoiceStopped,
			[this](const AsyncMixerEvent& ev) { HandleVoiceStoppedEvent(ev); });

		return Result::Ok;
	}

	void MixController::Shutdown() {
		if (!m_asyncMessageBus) return;

		m_asyncMessageBus->UnsubscribeFromMixerEvent(AsyncMixerEvent::Type::VoiceStopped);
		m_asyncMessageBus = nullptr;
	}

	void MixController::SyncPools() {
		m_voices.Sync();
		m_listeners.Sync();
		m_buses.Sync();
		m_biquads.Sync();
	}

	void MixController::OnSoundLoadComplete(SoundHandle sound, Result loadResult) {
		uint64_t targetRawId = sound.GetRawId();

		for (auto it = m_pendingPlaybacks.begin(); it != m_pendingPlaybacks.end();) {
			if (it->assetRawId != targetRawId) {
				++it;
				continue;
			}

			// Found pending playback
			PlaybackHandle playback = it->playback;
			it = m_pendingPlaybacks.erase(it);

			uint32_t vIndex = playback.GetIndex();
			VoiceMirror& vMirror = m_voices.GetMirror(vIndex);

			// Check if user already destroyed or stopped this playback
			if (vMirror.gen != playback.GetGeneration()) continue;

			if (loadResult == Result::Ok) {
				if (PrepareDeferredVoice(playback, sound) != Result::Ok) {
					ResetAndFreeVoice(vIndex);
				}
			}
			else {
				ResetAndFreeVoice(vIndex);
			}
		}
	}

	uint32_t MixController::AbortAndStopVoicesForSound(SoundHandle sound) {
		uint64_t soundRawId = sound.GetRawId();

		// Free pending playbacks
		for (auto it = m_pendingPlaybacks.begin(); it != m_pendingPlaybacks.end();) {
			if (it->assetRawId == soundRawId) {
				uint32_t vIndex = it->playback.GetIndex();
				VoiceMirror& vMirror = m_voices.GetMirror(vIndex);
				if (vMirror.gen == it->playback.GetGeneration()) ResetAndFreeVoice(vIndex);
				it = m_pendingPlaybacks.erase(it);
			}
			else {
				++it;
			}
		}

		// Command active voices using this sound to stop
		uint32_t activeVoicesStopped = 0;
		for (uint32_t i = 0; i < m_voices.Capacity(); ++i) {
			VoiceMirror& vMirror = m_voices.GetMirror(i);
			if (vMirror.gen != NO_GENERATION && vMirror.assetRawId == soundRawId) {
				vMirror.state = VoiceState::Stopped;
				m_asyncMessageBus->PushMixerCommand(AsyncMixerCommand::StopVoice(
					i, vMirror.gen
				));
				activeVoicesStopped++;
			}
		}

		return activeVoicesStopped;
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
			if (bMirror.effectSlots[slot] == effect) bMirror.effectSlots[slot] = InvalidEffectHandle;

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

	void MixController::HandleVoiceStoppedEvent(const AsyncMixerEvent& ev) {
		uint32_t vIndex = ev.data.voice.index;
		uint32_t vGen = ev.data.voice.gen;
		if (vIndex >= m_voices.Capacity()) return;

		VoiceMirror& vMirror = m_voices.GetMirror(vIndex);
		if (vMirror.gen != vGen) return;

		uint64_t releasedAssetRawId = vMirror.assetRawId;
		PlaybackExitCallback userCallback = std::move(vMirror.onStopCallback);

		vMirror.Reset();
		m_voices.Free(vIndex);

		if (m_assetController && releasedAssetRawId != INVALID_RAW_ID) {
			m_assetController->OnVoiceStopped(releasedAssetRawId);
		}

		// Invoke registered callback
		userCallback(PlaybackHandle::Create(vIndex, vGen), ev.data.voice.exitCondition);
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

		if (vMirror.state == VoiceState::Playing) {
			m_asyncMessageBus->PushMixerCommand(AsyncMixerCommand::PlayVoice(
				vIndex, vMirror.gen
			));
		}

		return Result::Ok;
	}

	void MixController::ResetAndFreeVoice(uint32_t vIndex) {
		m_voices.GetMirror(vIndex).Reset();
		m_voices.Free(vIndex);
	}
}
