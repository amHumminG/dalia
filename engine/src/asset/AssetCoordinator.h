#pragma once

#include "dalia/Result.h"
#include "dalia/SoundControl.h"
#include "asset/types/ResidentSound.h"
#include "asset/types/StreamSound.h"

#include <cstdint>
#include <memory>

namespace dalia {

	class AsyncMessageBus;
	class AssetRegistry;
	template<typename T> class AsyncPool;
	struct StreamContext;

	struct AsyncLoadCommand;
	struct AsyncLoadEvent;
	struct AsyncStreamCommand;

	/// @brief Represents a sound waiting on voices that use it to stop before it can be unloaded.
	struct PendingSoundUnload {
		SoundHandle handle;
		uint32_t remainingVoices = 0;
	};

	class AssetCoordinator {
	public:
		AssetCoordinator();
		~AssetCoordinator();

		Result Initialize(
			uint32_t residentSoundCapacity,
			uint32_t streamSoundCapacity,
			uint32_t streamCapacity,
			AsyncMessageBus* asyncMessageBus
		);

		void Shutdown();

		Result LoadSoundAsync(SoundHandle& sound, SoundType type, const char* filepath, AssetLoadCallback callback, uint32_t* requestId);
		Result ReleaseSoundRef(SoundHandle sound, bool& needsCleanup);
		void DeferOrFreeSound(SoundHandle sound, uint32_t activeVoiceCount);
		void OnVoiceStopped(uint64_t assetRawId);

		ResidentSound* GetResidentSound(SoundHandle handle);
		StreamSound* GetStreamSound(SoundHandle handle);
		Result AllocateAndPrepareStream(const char* filepath, uint32_t& streamIndex, uint32_t& streamGen);

	private:
		// --- Load Thread ---
		void HandleLoadSoundCommand(const AsyncLoadCommand& cmd);

		// --- Stream Thread ---
		void HandlePrepareStreamCommand(const AsyncStreamCommand& cmd);
		void HandleRefillStreamCommand(const AsyncStreamCommand& cmd);
		void HandleSeekStreamCommand(const AsyncStreamCommand& cmd);
		void HandleReleaseStreamCommand(const AsyncStreamCommand& cmd);

		static void FillStreamBuffer(StreamContext& stream, uint32_t bufferIndex);

		// --- Main Thread ---
		void HandleSoundLoadedEvent(const AsyncLoadEvent& ev);
		void HandleSoundLoadFailedEvent(const AsyncLoadEvent& ev);

		std::unique_ptr<AssetRegistry> m_assetRegistry;
		uint32_t nextIoLoadRequestId = 1;
		std::unordered_map<uint32_t, AssetLoadCallback> m_loadCallbacks;
		std::vector<PendingSoundUnload> m_pendingSoundUnloads;

		std::unique_ptr<AsyncPool<StreamContext>> m_streamPool;

		AsyncMessageBus* m_asyncMessageBus = nullptr;

	};
}

