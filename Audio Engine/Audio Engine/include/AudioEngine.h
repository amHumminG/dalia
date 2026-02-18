#pragma once
#include "Result.h"
#include <memory>

struct EngineConfig {
	uint32_t voiceCapacity = 128;
	uint32_t maxVoices = 64;

	uint32_t commandBufferCapacity = 1024;
	uint32_t eventBufferCapacity = 1024;
};

struct ma_device;
class CommandQueue;
class EventQueue;

namespace placeholder_name {

	class AudioEngine {
	public:
		AudioEngine() = default;
		~AudioEngine();

		Result Init(const EngineConfig& config);
		Result Deinit();

		void Update();

	private:
		static void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, uint32_t frameCount);

		void ProcessCommands();
		void Render(float* outputBuffer, uint32_t frameCount);

		static constexpr size_t MAX_VOICES = 128;
		//std::unique_ptr<Voice[]> m_voices;

		std::unique_ptr<CommandQueue> m_commandQueue;
		std::unique_ptr<EventQueue> m_eventQueue;

		bool m_initialized = false;
		std::unique_ptr<ma_device> m_device; // Miniaudio playback device
	};
}