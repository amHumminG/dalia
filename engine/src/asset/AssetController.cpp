#include "asset/AssetController.h"

#include "asset/AssetRegistry.h"
#include "asset/types/ResidentSound.h"
#include "asset/types/StreamSound.h"
#include "async/messaging/AsyncMessageBus.h"
#include "core/AsyncPool.h"
#include "mixer/StreamContext.h"
#include "io/codec/Codec.h"
#include "core/Logger.h"

namespace dalia {

	AssetController::AssetController(uint32_t residentSoundCapacity, uint32_t streamSoundCapacity)
		: m_assetRegistry(residentSoundCapacity, streamSoundCapacity) {
	}

	AssetController::~AssetController() {
		Shutdown();
	}

	Result AssetController::Initialize(AsyncMessageBus* asyncMessageBus, SoundLoadCompletionCallback onSoundLoad) {
		m_asyncMessageBus = asyncMessageBus;
		m_onSoundLoadCallback = std::move(onSoundLoad);

		// Subscribe to load events (Main Thread)
		m_asyncMessageBus->SubscribeToLoadEvent(AsyncLoadEvent::Type::SoundLoaded,
			[this](const AsyncLoadEvent& ev) { HandleSoundLoadedEvent(ev); });
		m_asyncMessageBus->SubscribeToLoadEvent(AsyncLoadEvent::Type::SoundLoadFailed,
			[this](const AsyncLoadEvent& ev) { HandleSoundLoadFailedEvent(ev); });

		return Result::Ok;
	}

	void AssetController::Shutdown() {
		if (!m_asyncMessageBus) return;

		// Unsubscribe from load events (Main Thread)
		m_asyncMessageBus->UnsubscribeFromLoadEvent(AsyncLoadEvent::Type::SoundLoaded);
		m_asyncMessageBus->UnsubscribeFromLoadEvent(AsyncLoadEvent::Type::SoundLoadFailed);

		m_asyncMessageBus = nullptr;
	}

	Result AssetController::LoadSoundAsync(SoundHandle& handle, SoundType type, const char* filepath,
		AssetLoadCallback callback, uint32_t* requestId) {

		if (!filepath || filepath[0] == '\0') return Result::InvalidArgs;

		const SoundID pathId(filepath);

		// Check if sound is already loaded
		if (m_assetRegistry.GetLoadedSoundHandle(pathId, handle)) {
			if (handle.GetType() == type) {
				// Increment ref count
				DALIA_LOG_DEBUG(LOG_CTX_API, "Sound from filepath: %s is already loaded. Incrementing reference count.",
					filepath);
				if (type == SoundType::Resident) {
					if (ResidentSound* sound = m_assetRegistry.GetResidentSound(handle)) sound->refCount++;
				}
				else if (type == SoundType::Stream) {
					if (StreamSound* sound = m_assetRegistry.GetStreamSound(handle)) sound->refCount++;
				}

				if (callback) callback(INVALID_REQUEST_ID, Result::Ok);
				if (requestId) *requestId = INVALID_REQUEST_ID;
				return Result::Ok;
			}
		}

		// Allocate new sound slot
		handle = m_assetRegistry.AllocateSound(type);
		if (!handle.IsValid()) {
			DALIA_LOG_ERR(LOG_CTX_API, "Failed to load sound from file: %s. Sound capacity reached.", filepath);
			return Result::SoundCapacityReached;
		}

		std::vector<float>* targetPcmBuffer = nullptr;
		if (type == SoundType::Resident) {
			ResidentSound* sound = m_assetRegistry.GetResidentSound(handle);
			sound->refCount = 1;
			sound->pathHash = pathId.GetHash();
			sound->state = LoadState::Loading;
			targetPcmBuffer = &sound->pcmData;
		}
		else {
			StreamSound* sound = m_assetRegistry.GetStreamSound(handle);
			sound->refCount = 1;
			sound->pathHash = pathId.GetHash();
			sound->state = LoadState::Loading;
			snprintf(sound->filepath, MAX_STR_LEN_ASSET_PATH, "%s", filepath);
		}

		m_assetRegistry.RegisterLoadedSound(pathId, handle);

		uint32_t reqId = nextIoLoadRequestId++;
		if (requestId) *requestId = reqId;
		if (callback) m_loadCallbacks[reqId] = std::move(callback);

		const char* typeStr = (type == SoundType::Resident) ? "Resident" : "Stream";
		DALIA_LOG_DEBUG(LOG_CTX_API, "Queued async sound load (%s) from %s [ReqID: %u, Callback: %s]",
			typeStr, filepath, requestId, callback ? "Yes" : "No");

		// Publish load command
		AsyncLoadCommand cmd = AsyncLoadCommand::LoadSound(reqId, handle.GetRawId(), filepath, targetPcmBuffer);
		if (m_asyncMessageBus->PushLoadCommand(cmd)) {
			// Publish failed -> Rollback
			if (callback) m_loadCallbacks.erase(reqId);
			m_assetRegistry.UnregisterLoadedSound(pathId);
			m_assetRegistry.FreeSound(handle);
			handle = INVALID_SOUND_HANDLE;

			DALIA_LOG_ERR(LOG_CTX_API, "Failed to queue sound load from file: %s. Load queue capacity reached.",
				filepath);
			return Result::LoadQueueCapacityReached;
		}

		return Result::Ok;
	}

	Result AssetController::GetSoundLength(SoundHandle handle, double& lengthInSeconds) {
		if (handle.GetType() == SoundType::Resident) {
			ResidentSound* sound = m_assetRegistry.GetResidentSound(handle);
			if (!sound || sound->state != LoadState::Loaded) return Result::InvalidHandle;
			lengthInSeconds = sound->lengthInSeconds;
		}
		else if (handle.GetType() == SoundType::Stream) {
			StreamSound* sound = m_assetRegistry.GetStreamSound(handle);
			if (!sound || sound->state != LoadState::Loaded) return Result::InvalidHandle;
			lengthInSeconds = sound->lengthInSeconds;
		}
		return Result::Ok;
	}

	Result AssetController::ReleaseSoundRef(SoundHandle handle, bool& readyToUnload) {
		readyToUnload = false;
		uint32_t refCount = 0;
		uint32_t pathHash = 0;

		if (handle.GetType() == SoundType::Resident) {
			ResidentSound* sound = m_assetRegistry.GetResidentSound(handle);
			if (!sound) return Result::InvalidHandle;
			if (sound->refCount > 0) sound->refCount--;
			refCount = sound->refCount;
			pathHash = sound->pathHash;
		}
		else if (handle.GetType() == SoundType::Stream) {
			StreamSound* sound = m_assetRegistry.GetStreamSound(handle);
			if (!sound) return Result::InvalidHandle;
			if (sound->refCount > 0) sound->refCount--;
			refCount = sound->refCount;
			pathHash = sound->pathHash;
		}

		if (refCount == 0) {
			m_assetRegistry.UnregisterLoadedSound(SoundID::FromHash(pathHash));
			readyToUnload = true;
		}

		return Result::Ok;
	}

	void AssetController::FinalizeOrDeferUnload(SoundHandle handle, uint32_t activeVoiceCount) {
		if (activeVoiceCount == 0) {
			m_assetRegistry.FreeSound(handle);
			DALIA_LOG_DEBUG(LOG_CTX_API, "Unloaded sound (immediately). Handle: 0x%016llx.", handle.GetRawId());
		}
		else {
			m_pendingSoundUnloads.push_back({ handle, activeVoiceCount });
			DALIA_LOG_DEBUG(LOG_CTX_API, "Deferring sound unload, waiting on %u voice(s). Handle: 0x%016llx.",
				activeVoiceCount, handle.GetRawId());
		}
	}

	void AssetController::OnVoiceStopped(uint64_t assetRawId) {
		for (auto it = m_pendingSoundUnloads.begin(); it != m_pendingSoundUnloads.end(); ) {
			if (it->handle.GetRawId() == assetRawId) {
				if (it->remainingVoices > 0) it->remainingVoices--;

				if (it->remainingVoices == 0) {
					m_assetRegistry.FreeSound(it->handle);
					DALIA_LOG_DEBUG(LOG_CTX_CORE, "Unloaded sound (deferred). Handle: 0x%016llx.", assetRawId);
					it = m_pendingSoundUnloads.erase(it);
					return;
				}
			}
			++it;
		}
	}

	ResidentSound* AssetController::GetResidentSound(SoundHandle handle) {
		return m_assetRegistry.GetResidentSound(handle);
	}

	StreamSound* AssetController::GetStreamSound(SoundHandle handle) {
		return m_assetRegistry.GetStreamSound(handle);
	}

	void AssetController::HandleSoundLoadedEvent(const AsyncLoadEvent& ev) {
		SoundHandle handle = SoundHandle::FromRawId(ev.assetRawId);

		if (handle.GetType() == SoundType::Resident) {
			if (ResidentSound* sound = m_assetRegistry.GetResidentSound(handle)) {
				sound->channels = ev.formatInfo.channels;
				sound->sampleRate = ev.formatInfo.sampleRate;
				sound->totalFrames = ev.formatInfo.totalFrames;
				sound->lengthInSeconds = ev.formatInfo.lengthInSeconds;
				sound->state = LoadState::Loaded;
			}
			// TODO: Log error here
		}
		else if (handle.GetType() == SoundType::Stream) {
			if (StreamSound* sound = m_assetRegistry.GetStreamSound(handle)) {
				sound->channels = ev.formatInfo.channels;
				sound->sampleRate = ev.formatInfo.sampleRate;
				sound->totalFrames = ev.formatInfo.totalFrames;
				sound->lengthInSeconds = ev.formatInfo.lengthInSeconds;
				sound->state = LoadState::Loaded;
			}
			// TODO: Log error here
		}

		// Notify MixController
		if (m_onSoundLoadCallback) m_onSoundLoadCallback(handle, Result::Ok);

		// Fire user load callback
		if (auto it = m_loadCallbacks.find(ev.requestId); it != m_loadCallbacks.end()) {
			if (it->second) it->second(ev.requestId, Result::Ok);
			m_loadCallbacks.erase(it);
		}
	}

	void AssetController::HandleSoundLoadFailedEvent(const AsyncLoadEvent& ev) {
		SoundHandle handle = SoundHandle::FromRawId(ev.assetRawId);

		if (handle.GetType() == SoundType::Resident) {
			if (ResidentSound* sound = m_assetRegistry.GetResidentSound(handle)) sound->state = LoadState::Error;
			// TODO: Log error here
		}
		else if (handle.GetType() == SoundType::Stream) {
			if (StreamSound* sound = m_assetRegistry.GetStreamSound(handle)) sound->state = LoadState::Error;
			// TODO: Log error here
		}

		// Notify MixController
		if (m_onSoundLoadCallback) m_onSoundLoadCallback(handle, ev.result);

		// Fire user load callback
		if (auto it = m_loadCallbacks.find(ev.requestId); it != m_loadCallbacks.end()) {
			if (it->second) it->second(ev.requestId, ev.result);
			m_loadCallbacks.erase(it);
		}
	}
}
