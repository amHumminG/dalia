#pragma once
#include "dalia/core/Result.h"
#include "dalia/core/LogLevel.h"
#include <memory>

struct ma_device;

namespace dalia {

	struct EngineInternalState;
	struct PlaybackHandle;

	struct EngineConfig {
		LogLevel logLevel = LogLevel::Warning;
		LogCallback logCallback = nullptr;

		uint32_t voiceCapacity		= 128;
		uint32_t maxActiveVoices	= 64;
		uint32_t streamCapacity		= 32;
		uint32_t busCapacity		= 64;

		size_t rtCommandQueueCapacity		= 1024;
		size_t rtEventQueueCapacity			= 1024;
		size_t ioStreamRequestQueueCapacity	= 256;
	};

	class Engine {
	public:
		Engine();
		~Engine();

		Result Init(const EngineConfig& config);
		Result Deinit();

		void Update();

		Result CreateStreamPlayback(PlaybackHandle& handle, const char* filepath);
		Result Play(PlaybackHandle handle);

	private:
		void TryUpdateMixOrder();
		static void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, uint32_t frameCount);

		std::unique_ptr<EngineInternalState> m_state;
	};
}