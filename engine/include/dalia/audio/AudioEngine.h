#pragma once
#include "dalia/core/Result.h"
#include "dalia/core/LogLevel.h"
#include <memory>

struct ma_device;

namespace dalia {

	struct EngineInternalState;
	struct Voice;
	class Bus;

	struct EngineConfig {
		LogLevel logLevel = LogLevel::Warning;

		uint32_t voiceCapacity = 128;
		uint32_t maxActiveVoices = 64;
		uint32_t streamCapacity = 32;

		uint32_t busCapacity = 64;

		size_t commandQueueCapacity = 1024;
		size_t eventQueueCapacity = 1024;
		size_t ioQueueCapacity = 256;
	};

	class AudioEngine {
	public:
		AudioEngine();
		~AudioEngine();

		Result Init(const EngineConfig& config);
		Result Deinit();

		void Update();

	private:
		static void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, uint32_t frameCount);
		void IoThreadMain();

		void ProcessCommands();
		void Render(float* outputBuffer, uint32_t frameCount);
		bool MixVoiceToBus(Voice& voice, uint32_t busIndex, uint32_t frameCount);
		void BuildBusGraph();

		std::unique_ptr<EngineInternalState> m_state;
	};
}