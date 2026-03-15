#pragma once
#include "dalia/core/Result.h"
#include "dalia/core/LogLevel.h"
#include "PlaybackControl.h"
#include "dalia/audio/ResourceHandle.h"
#include <memory>

struct ma_device;

namespace dalia {

	class StringID;
	struct EngineInternalState;
	struct PlaybackHandle;

	struct RtEvent;
	struct IoLoadEvent;

	struct EngineConfig {
		LogLevel logLevel = LogLevel::Warning;
		LogCallback logCallback = nullptr;

		uint32_t residentSoundCapacity = 256; // Maybe this should be higher?
		uint32_t streamSoundCapacity = 256;

		uint32_t voiceCapacity		= 128;
		uint32_t maxActiveVoices	= 64;
		uint32_t streamCapacity		= 32;
		uint32_t busCapacity		= 64;

		size_t rtCommandQueueCapacity		= 1024;
		size_t rtEventQueueCapacity			= 1024;
		size_t ioStreamRequestQueueCapacity	= 256;
		size_t ioLoadRequestQueueCapacity	= 64;
		size_t ioLoadEventQueueCapacity		= 64;
	};

	class Engine {
	public:
		Engine();
		~Engine();

		Result Init(const EngineConfig& config);
		Result Deinit();

		void Update();



		// --- Loose File Functionality ---

		Result LoadResidentSound(ResidentSoundHandle& soundHandle, const char* filepath,
			AssetLoadCallback callback = nullptr, uint32_t* outRequestId = nullptr);
		Result LoadStreamSound(StreamSoundHandle& soundHandle, const char* filepath,
			AssetLoadCallback callback = nullptr, uint32_t* outRequestId = nullptr);

		Result Unload(ResidentSoundHandle soundHandle);
		Result Unload(StreamSoundHandle soundHandle);

		Result CreatePlayback(PlaybackHandle& pbkHandle, ResidentSoundHandle soundHandle);
		Result CreatePlayback(PlaybackHandle& pbkHandle, StreamSoundHandle soundHandle);


		// --- Soundbank Functionality ---

		// Result LoadBank(BankHandle& handle, StringID pathId);
		// Result Unload(BankHandle handle);

		// Result PostEvent(EventHandle& handle, StringID);




		// Result CreateStreamPlayback(PlaybackHandle& handle, const char* filepath);
		Result Play(PlaybackHandle handle);

	private:
		void ProcessRtEvent(const RtEvent& ev);
		void ProcessIoLoadEvent(const IoLoadEvent& ev);
		void TryUpdateMixOrder();
		static void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, uint32_t frameCount);

		std::unique_ptr<EngineInternalState> m_state;
	};
}