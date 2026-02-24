#pragma once
#include "Result.h"
#include "LogLevel.h"
#include <memory>

struct ma_device;

namespace dalia {

	struct Voice;
	struct VoiceSlot;
	struct StreamingContext;
	class Bus;

	struct EngineConfig {
		LogLevel logLevel = LogLevel::Warning;

		uint32_t voiceCapacity = 128;
		uint32_t maxActiveVoices = 64;
		uint32_t streamCapacity = 32;

		uint32_t busCapacity = 64;

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
		bool MixVoiceToBus(Voice& voice, Bus* bus, uint32_t frameCount);

		bool m_initialized = false;
		std::unique_ptr<ma_device> m_device; // Miniaudio playback device

		// Thread communication queues
		std::unique_ptr<CommandQueue>	m_commandQueue;
		std::unique_ptr<EventQueue>		m_eventQueue;

		// Resource Capacities
		uint32_t m_voiceCapacity	= 0;
		uint32_t m_streamCapacity	= 0;
		uint32_t m_busCapacity		= 0;

		// Pools
		std::unique_ptr<Voice[]>			m_voicePool;
		std::unique_ptr<StreamingContext[]> m_streamPool;
		std::unique_ptr<Bus[]>				m_busPool;
		std::unique_ptr<float[]>			m_busMemoryPool;

		// References
		Bus* m_masterBus = nullptr;
	};
}