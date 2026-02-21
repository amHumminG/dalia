#pragma once
#include "Result.h"
#include "LogLevel.h"
#include <memory>

struct ma_device;

namespace dalia {

	struct EngineConfig {
		LogLevel logLevel = LogLevel::Warning;

		uint32_t voiceCapacity = 128;
		uint32_t maxVoices = 64;

		uint32_t commandBufferCapacity = 1024;
		uint32_t eventBufferCapacity = 1024;
	};

	class CommandQueue;
	class EventQueue;

	class AudioEngine {
	public:
		AudioEngine();
		~AudioEngine();

		Result Init(const EngineConfig& config);
		Result Deinit();

		void Update();

	private:
		static void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, uint32_t frameCount);

		void ProcessCommands();
		void Render(float* outputBuffer, uint32_t frameCount);

		std::unique_ptr<CommandQueue> m_commandQueue;
		std::unique_ptr<EventQueue> m_eventQueue;

		bool m_initialized = false;
		std::unique_ptr<ma_device> m_device; // Miniaudio playback device
	};
}