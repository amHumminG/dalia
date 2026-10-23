#pragma once

#include "dalia/Result.h"
#include "dalia/SoundControl.h"
#include "asset/types/ResidentSound.h"
#include "asset/types/StreamSound.h"
#include "asset/AssetRegistry.h"

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

	class AssetController {
	public:
		using SoundLoadCompletionCallback = std::function<void(SoundHandle handle, Result result)>;

		AssetController(uint32_t residentSoundCapacity, uint32_t streamSoundCapacity);
		~AssetController();

		Result Initialize(
			AsyncMessageBus* asyncMessageBus,
			SoundLoadCompletionCallback onSoundLoad
		);
		void Shutdown();

		Result LoadSoundAsync(SoundHandle& handle, SoundType type, const char* filepath, AssetLoadCallback callback,
			uint32_t* requestId);

		Result GetSoundLength(SoundHandle handle, double& lengthInSeconds);

		Result ReleaseSoundRef(SoundHandle handle, bool& readyToUnload);

		void FinalizeOrDeferUnload(SoundHandle handle, uint32_t activeVoiceCount);

		void OnVoiceStopped(uint64_t assetRawId);

		[[nodiscard]] ResidentSound* GetResidentSound(SoundHandle handle);

		[[nodiscard]] StreamSound* GetStreamSound(SoundHandle handle);
	private:
		// --- Main Thread ---
		void HandleSoundLoadedEvent(const AsyncLoadEvent& ev);
		void HandleSoundLoadFailedEvent(const AsyncLoadEvent& ev);

		AssetRegistry m_assetRegistry;
		AsyncMessageBus* m_asyncMessageBus = nullptr;
		SoundLoadCompletionCallback m_onSoundLoadCallback;

		uint32_t nextIoLoadRequestId = 1;
		std::unordered_map<uint32_t, AssetLoadCallback> m_loadCallbacks;
		std::vector<PendingSoundUnload> m_pendingSoundUnloads;
	};
}

