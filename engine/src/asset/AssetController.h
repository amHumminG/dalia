#pragma once

#include "dalia/Result.h"
#include "dalia/SoundControl.h"
#include "asset/types/ResidentSound.h"
#include "asset/types/StreamSound.h"
#include "asset/AssetRegistry.h"
#include "io/codec/Codec.h"

#include <cstdint>
#include <memory>

namespace dalia {

	class AsyncMessageBus;

	struct AsyncLoadCommand;
	struct AsyncLoadEvent;

	class AssetController {
	public:
		AssetController(uint32_t residentSoundCapacity, uint32_t streamSoundCapacity);
		~AssetController();

		Result Initialize(AsyncMessageBus* asyncMessageBus);
		void Shutdown();

		// === Asset API ===
		Result LoadSoundAsync(SoundHandle& sound, SoundType type, const char* filepath, AssetLoadCallback callback,
			uint32_t* requestId);
		Result UnloadSound(SoundHandle sound);

		Result GetSoundDescriptor(SoundHandle sound, SoundDescriptor& soundDesc);

		bool AcquireSound(SoundHandle sound);
		void ReleaseSound(SoundHandle sound);

		void FinalizeAsyncSoundLoad(SoundHandle sound, const AudioFormatInfo& info, const char* filepath = nullptr);
		void FailSoundLoad(SoundHandle sound);
		void InvokeLoadCallback(uint32_t requestId, Result result);

	private:
		[[nodiscard]] ResidentSound* GetResidentSound(SoundHandle sound);
		[[nodiscard]] StreamSound* GetStreamSound(SoundHandle sound);

		AsyncMessageBus* m_asyncMessageBus = nullptr;
		AssetRegistry m_assetRegistry;

		uint32_t nextIoLoadRequestId = 1;
		std::unordered_map<uint32_t, AssetLoadCallback> m_loadCallbacks;
	};
}

