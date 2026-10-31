#include "asset/AssetController.h"

#include <future>

#include "asset/AssetRegistry.h"
#include "asset/types/ResidentSound.h"
#include "asset/types/StreamSound.h"
#include "async/messaging/AsyncMessageBus.h"
#include "core/AsyncPool.h"
#include "../mixer/nodes/StreamContext.h"
#include "io/codec/Codec.h"
#include "core/Logger.h"

namespace dalia {

	AssetController::AssetController(uint32_t residentSoundCapacity, uint32_t streamSoundCapacity)
		: m_assetRegistry(residentSoundCapacity, streamSoundCapacity) {
	}

	AssetController::~AssetController() {
		Shutdown();
	}

	Result AssetController::Initialize(AsyncMessageBus* asyncMessageBus) {
		m_asyncMessageBus = asyncMessageBus;

		return Result::Ok;
	}

	void AssetController::Shutdown() {
		if (!m_asyncMessageBus) return;

		m_asyncMessageBus = nullptr;
	}

	Result AssetController::LoadSoundAsync(SoundHandle& sound, SoundType type, const char* filepath,
		AssetLoadCallback callback, uint32_t* requestId) {

		if (!filepath || filepath[0] == '\0') return Result::InvalidArgs;

		const SoundID pathId(filepath);

		// Check if sound is already loaded
		if (m_assetRegistry.GetLoadedSoundHandle(pathId, sound)) {
			if (sound.GetType() == type && AcquireSound(sound)) {
				if (callback) callback(INVALID_REQUEST_ID, Result::Ok);
				if (requestId) *requestId = INVALID_REQUEST_ID;
				return Result::Ok;
			}
		}

		// Allocate new sound slot
		sound = m_assetRegistry.AllocateSound(type);
		if (!sound.IsValid()) {
			DALIA_LOG_ERR(LOG_CTX_API, "Failed to load sound from file: %s. Sound capacity reached.", filepath);
			return Result::SoundCapacityReached;
		}

		std::vector<float>* targetPcmBuffer = nullptr;

		if (type == SoundType::Resident) {
			ResidentSound* rs = m_assetRegistry.GetResidentSound(sound);
			rs->refCount = 2;
			rs->pathHash = pathId.GetHash();
			rs->state = LoadState::Loading;
			targetPcmBuffer = &rs->pcmData;
		}
		else {
			StreamSound* ss = m_assetRegistry.GetStreamSound(sound);
			ss->refCount = 2;
			ss->pathHash = pathId.GetHash();
			ss->state = LoadState::Loading;
			snprintf(ss->filepath, MAX_STR_LEN_ASSET_PATH, "%s", filepath);
		}

		m_assetRegistry.RegisterLoadedSound(pathId, sound);

		uint32_t reqId = nextIoLoadRequestId++;
		if (requestId) *requestId = reqId;
		if (callback) m_loadCallbacks[reqId] = std::move(callback);

		const char* typeStr = (type == SoundType::Resident) ? "Resident" : "Stream";
		DALIA_LOG_DEBUG(LOG_CTX_API, "Queued async sound load (%s) from %s [ReqID: %u, Callback: %s]",
			typeStr, filepath, requestId, callback ? "Yes" : "No");

		// Publish load command
		AsyncLoadCommand cmd = AsyncLoadCommand::LoadSound(reqId, sound.GetRawId(), filepath, targetPcmBuffer);
		if (!m_asyncMessageBus->PushLoadCommand(cmd)) {
			// Publish failed -> Rollback
			if (callback) m_loadCallbacks.erase(reqId);
			m_assetRegistry.UnregisterLoadedSound(pathId);
			m_assetRegistry.FreeSound(sound);
			sound = INVALID_SOUND_HANDLE;

			DALIA_LOG_ERR(LOG_CTX_API, "Failed to queue sound load from file: %s. Load queue capacity reached.",
				filepath);
			return Result::LoadQueueCapacityReached;
		}

		return Result::Ok;
	}

	Result AssetController::UnloadSound(SoundHandle sound) {
		if (!sound.IsValid()) return Result::InvalidHandle;

		uint32_t pathHash = 0;
		if (sound.GetType() == SoundType::Resident) {
			ResidentSound* rs = m_assetRegistry.GetResidentSound(sound);
			if (!rs) return Result::InvalidHandle;
			pathHash = rs->pathHash;
		}
		else {
			StreamSound* ss = m_assetRegistry.GetStreamSound(sound);
			if (!ss) return Result::InvalidHandle;
			pathHash = ss->pathHash;
		}

		if (pathHash != 0) m_assetRegistry.UnregisterLoadedSound(SoundID::FromHash(pathHash));

		ReleaseSound(sound);
		return Result::Ok;
	}

	Result AssetController::GetSoundDescriptor(SoundHandle sound, SoundDescriptor& soundDesc) {
		return m_assetRegistry.GetSoundDescriptor(sound, soundDesc);
	}

	bool AssetController::AcquireSound(SoundHandle sound) {
		if (sound.GetType() == SoundType::Resident) {
			ResidentSound* rs = m_assetRegistry.GetResidentSound(sound);
			if (!rs || rs->refCount == 0) return false;
			rs->refCount++;
			return true;
		}
		else {
			StreamSound* ss = m_assetRegistry.GetStreamSound(sound);
			if (!ss || ss->refCount == 0) return false;
			ss->refCount++;
			return true;
		}
	}

	void AssetController::ReleaseSound(SoundHandle sound) {
		if (!sound.IsValid()) return;

		if (sound.GetType() == SoundType::Resident) {
			ResidentSound* rs = m_assetRegistry.GetResidentSound(sound);
			if (!rs || rs->refCount == 0) return;
			rs->refCount--;
			if (rs->refCount == 0) {
				m_assetRegistry.FreeSound(sound);
				DALIA_LOG_DEBUG(LOG_CTX_CORE, "Freed resident sound. Handle: 0x%016llx.", sound.GetRawId());
			}
		}
		else {
			StreamSound* ss = m_assetRegistry.GetStreamSound(sound);
			if (!ss || ss->refCount == 0) return;
			ss->refCount--;
			if (ss->refCount == 0) {
				m_assetRegistry.FreeSound(sound);
				DALIA_LOG_DEBUG(LOG_CTX_CORE, "Freed stream sound. Handle: 0x%016llx.", sound.GetRawId());
			}
		}
	}

	void AssetController::FinalizeAsyncSoundLoad(SoundHandle sound, const AudioFormatInfo& info, const char* filepath) {
		if (sound.GetType() == SoundType::Resident) {
			if (ResidentSound* rs = m_assetRegistry.GetResidentSound(sound)) {
				rs->channels = info.channels;
				rs->sampleRate = info.sampleRate;
				rs->totalFrames = info.totalFrames;
				rs->lengthInSeconds = info.lengthInSeconds;
				rs->state = LoadState::Loaded;
			}
		}
		else if (sound.GetType() == SoundType::Stream) {
			if (StreamSound* ss = m_assetRegistry.GetStreamSound(sound)) {
				snprintf(ss->filepath, MAX_STR_LEN_ASSET_PATH, "%s", filepath);
				ss->channels = info.channels;
				ss->sampleRate = info.sampleRate;
				ss->totalFrames = info.totalFrames;
				ss->lengthInSeconds = info.lengthInSeconds;
				ss->state = LoadState::Loaded;
			}
		}

		ReleaseSound(sound); // Release async load reference
	}

	void AssetController::FailSoundLoad(SoundHandle sound) {
		if (sound.GetType() == SoundType::Resident) {
			if (ResidentSound* rs = m_assetRegistry.GetResidentSound(sound)) rs->state = LoadState::Error;
		}
		else if (sound.GetType() == SoundType::Stream) {
			if (StreamSound* ss = m_assetRegistry.GetStreamSound(sound)) ss->state = LoadState::Error;
		}

		ReleaseSound(sound); // Release async load reference
	}

	void AssetController::InvokeLoadCallback(uint32_t requestId, Result result) {
		if (auto it = m_loadCallbacks.find(requestId); it != m_loadCallbacks.end()) {
			AssetLoadCallback cb = std::move(it->second);
			m_loadCallbacks.erase(it);
			if (cb) cb(requestId, result);
		}
	}

	ResidentSound* AssetController::GetResidentSound(SoundHandle handle) {
		return m_assetRegistry.GetResidentSound(handle);
	}

	StreamSound* AssetController::GetStreamSound(SoundHandle handle) {
		return m_assetRegistry.GetStreamSound(handle);
	}
}
