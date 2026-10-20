#include "dalia/Engine.h"
#include "engine/EngineInternalState.h"
#include "core/Logger.h"
#include "resources/AssetRegistry.h"
#include "async/AsyncLoadSystem.h"

namespace dalia {

	Result Engine::LoadSoundAsync(SoundHandle& sound, SoundType soundType, const char* filepath, AssetLoadCallback callback,
	                              uint32_t* outRequestId) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;

		const SoundID pathId(filepath);

		// Duplicate check
		if (m_state->assetRegistry->GetLoadedSoundHandle(pathId, sound)) {
			if (sound.GetType() == soundType) {
				if (soundType == SoundType::Resident) {
					ResidentSound* residentSound = m_state->assetRegistry->GetResidentSound(sound);
					if (residentSound) residentSound->refCount++;
				}
				else if (soundType == SoundType::Stream) {
					StreamSound* streamSound = m_state->assetRegistry->GetStreamSound(sound);
					if (streamSound) streamSound->refCount++;
				}

				if (callback) callback(INVALID_REQUEST_ID, Result::Ok);
				if (outRequestId) *outRequestId = INVALID_REQUEST_ID;

				DALIA_LOG_DEBUG(LOG_CTX_API, "Sound from filepath: %s is already loaded. Incremented reference count.",
					filepath);

				return Result::Ok;
			}
		}

		sound = m_state->assetRegistry->AllocateSound(soundType);
		if (!sound.IsValid()) {
			if (soundType == SoundType::Resident) {
				DALIA_LOG_ERR(LOG_CTX_API, "Failed to load sound %s. Resident sound pool capacity reached.", filepath);
			}
			else if (soundType == SoundType::Stream) {
				DALIA_LOG_ERR(LOG_CTX_API, "Failed to load sound %s. Stream sound pool capacity reached.", filepath);
			}
			return Result::SoundCapacityReached;
		}

		if (soundType == SoundType::Resident) {
			ResidentSound* residentSound = m_state->assetRegistry->GetResidentSound(sound);
			residentSound->refCount = 1;
			residentSound->pathHash = pathId.GetHash();
			residentSound->state.store(LoadState::Loading, std::memory_order_relaxed);
		}
		else if (soundType == SoundType::Stream) {
			StreamSound* streamSound = m_state->assetRegistry->GetStreamSound(sound);
			streamSound->refCount = 1;
			streamSound->pathHash = pathId.GetHash();
			streamSound->state.store(LoadState::Loading, std::memory_order_relaxed);
		}
		m_state->assetRegistry->RegisterLoadedSound(pathId, sound);

		uint32_t requestId = m_state->GenerateIoLoadRequestId();
		if (outRequestId) *outRequestId = requestId;
		if (callback) {
			m_state->loadCallbacks[requestId] = std::move(callback);
		}

		const char* typeStr = (soundType == SoundType::Resident) ? "Resident" : "Stream";
		DALIA_LOG_DEBUG(LOG_CTX_API, "Queued async sound load (%s) from %s [ReqID: %u, Callback: %s]",
			typeStr, filepath, requestId, callback ? "Yes" : "No");

		bool wasEmpty = false;
		if (m_state->asyncLoadRequests->Push(AsyncLoadCommand::LoadSound(requestId, sound, filepath), wasEmpty)) {
			if (wasEmpty) m_state->asyncLoadSystem->NotifyTaskAdded();
		}
		else {
			DALIA_LOG_ERR(LOG_CTX_API, "Failed to queue async sound load from %s. Queue is full.", filepath);
		}

		return Result::Ok;
	}

	Result Engine::UnloadSound(SoundHandle sound) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;

		SoundType soundType = sound.GetType();
		uint32_t soundRefCount;
		uint32_t soundPathHash;

		if (soundType == SoundType::Resident) {
			ResidentSound* residentSound = m_state->assetRegistry->GetResidentSound(sound);
			if (!residentSound) return Result::InvalidHandle;
			if (residentSound->refCount > 0) residentSound->refCount--;
			soundRefCount = residentSound->refCount;
			soundPathHash = residentSound->pathHash;
		}
		else {
			StreamSound* streamSound = m_state->assetRegistry->GetStreamSound(sound);
			if (!streamSound) return Result::InvalidHandle;
			if (streamSound->refCount > 0) streamSound->refCount--;
			soundRefCount = streamSound->refCount;
			soundPathHash = streamSound->pathHash;
		}

		if (soundRefCount == 0) {
			m_state->assetRegistry->UnregisterLoadedSound(SoundID::FromHash(soundPathHash));

			PendingSoundUnload pendingUnload;
			pendingUnload.handle = sound;

			// Remove pending playbacks for the sound
			for (auto it = m_state->pendingPlaybacks.begin(); it != m_state->pendingPlaybacks.end(); ) {
				if (it->assetRawId == sound.GetRawId()) {
					VoiceMirror& vMirror = m_state->voices.GetMirror(it->playback.GetIndex());

					if (vMirror.onStopCallback) {
						vMirror.onStopCallback(it->playback, PlaybackExitCondition::ExplicitStop);
					}

					vMirror.Reset();
					m_state->voices.Free(it->playback.GetIndex());

					DALIA_LOG_DEBUG(LOG_CTX_API, "Aborted deferred playback for voice %d due to early unload.",
						it->playback.GetIndex());
					it = m_state->pendingPlaybacks.erase(it);
				}
				else it++;
			}

			// Command active voices using the asset to stop
			for (uint32_t i = 0; i < m_state->voices.Capacity(); i++) {
				VoiceMirror& vMirror = m_state->voices.GetMirror(i);

				if (vMirror.state != VoiceState::Free && vMirror.assetRawId == sound.GetRawId()) {
					pendingUnload.voicesToStop.push_back(EngineInternalState::ForgePlaybackHandle(i, vMirror.gen));

					m_state->mixerCommands->Enqueue(AsyncMixerCommand::StopVoice(i, vMirror.gen));
					DALIA_LOG_DEBUG(LOG_CTX_API, "Commanded to stop voice %d.", i);
				}
			}

			if (!pendingUnload.voicesToStop.empty()) {
				m_state->pendingSoundUnloads.push_back(std::move(pendingUnload));
				DALIA_LOG_DEBUG(LOG_CTX_API,
					"Deferring sound unload (handle rawId: 0x%016llx) until playbacks have been explicitly stopped.");
			}
			else {
				m_state->assetRegistry->FreeSound(sound);
				DALIA_LOG_DEBUG(LOG_CTX_API, "Unloaded sound (handle rawId: 0x%016llx.", sound.GetRawId());
			}
		}

		return Result::Ok;
	}

	Result Engine::GetSoundLength(SoundHandle sound, double& lengthInSeconds) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;

		SoundType soundType = sound.GetType();

		if (soundType == SoundType::Resident) {
			ResidentSound* residentSound = m_state->assetRegistry->GetResidentSound(sound);
			if (!residentSound) return Result::InvalidHandle;
			lengthInSeconds = residentSound->lengthInSeconds;
		}
		else {
			StreamSound* streamSound = m_state->assetRegistry->GetStreamSound(sound);
			if (!streamSound) return Result::InvalidHandle;
			lengthInSeconds = streamSound->lengthInSeconds;
		}

		return Result::Ok;
	}
}
