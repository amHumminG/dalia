#pragma once

#include "dalia/Result.hpp"
#include "dalia/Sound.hpp"
#include "asset/types/ResidentSound.hpp"
#include "asset/types/StreamSound.hpp"
#include "asset/AssetRegistry.hpp"
#include "io/codec/Codec.hpp"

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

		void FinalizeAsyncSoundLoad(SoundHandle sound, const AudioFormatInfo& info);
		void FailSoundLoad(SoundHandle sound);
		void InvokeLoadCallback(uint32_t requestId, Result result);

	private:
		[[nodiscard]] ResidentSound* GetResidentSound(SoundHandle sound);
		[[nodiscard]] StreamSound* GetStreamSound(SoundHandle sound);

		bool m_initialized                 = false;
		AsyncMessageBus* m_asyncMessageBus = nullptr;
		AssetRegistry m_assetRegistry;

		uint32_t m_nextIoLoadRequestId = 1;
		std::unordered_map<uint32_t, AssetLoadCallback> m_loadCallbacks;
	};
}
