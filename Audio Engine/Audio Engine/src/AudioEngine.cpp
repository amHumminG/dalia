#include "AudioEngine.h"
#include "Logger.h"
#include "CommandQueue.h"
#include "EventQueue.h"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio/miniaudio.h"

namespace placeholder_name {

	AudioEngine::AudioEngine() = default;

	AudioEngine::~AudioEngine() {
		// Any manual deallocations here
	}

	Result AudioEngine::Init(const EngineConfig& config) {
		if (m_initialized) return Result::AlreadyInitialized;

		Logger::Init(config.logLevel, 256);

		// Miniaudio setup
		m_device = std::make_unique<ma_device>();
		ma_device_config deviceConfig = ma_device_config_init(ma_device_type_playback);
		deviceConfig.playback.format = ma_format_f32;
		deviceConfig.playback.channels = 2; // Stereo playback only
		deviceConfig.sampleRate = 44100; // Hz
		deviceConfig.dataCallback = data_callback;
		deviceConfig.pUserData = this;

		if (ma_device_init(NULL, &deviceConfig, m_device.get()) != MA_SUCCESS) {
			Logger::Log(LogLevel::Critical, "Device", "Failed to initialize playback device");
			return Result::DeviceFailed;
		}

		if (ma_device_start(m_device.get()) != MA_SUCCESS) {
			Logger::Log(LogLevel::Critical, "Device", "Failed to start playback device");
			return Result::DeviceFailed;
		}

		// Queue setup
		m_commandQueue = std::make_unique<CommandQueue>(config.commandBufferCapacity);
		m_eventQueue = std::make_unique<EventQueue>(config.eventBufferCapacity);

		Logger::Log(LogLevel::Info, "Engine", "Initialized Audio Engine");
		m_initialized = true;
		return Result::Ok;
	}

	Result AudioEngine::Deinit() {
		if (!m_initialized) return Result::NotInitialized;

		// Miniaudio teardown
		if (ma_device_stop(m_device.get()) != MA_SUCCESS) {
			Logger::Log(LogLevel::Critical, "Device", "Failed to stop playback device");
			return Result::DeviceFailed;
		}
		ma_device_uninit(m_device.get());

		m_initialized = false;
		Logger::Log(LogLevel::Info, "Engine", "Deinitialized Audio Engine");
		Logger::ProcessLogs(); // Drain the log buffer before deinit

		return Result::Ok;
	}

	void AudioEngine::Update() {
		// Process AudioEvents
		AudioEvent ev;
		while (m_eventQueue->Pop(ev)) {
			switch (ev.type) {
				case AudioEvent::Type::SoundFinished: {
					// TODO: Implement
				}
				case AudioEvent::Type::PlaybackError: {
					// TODO: Implement
				}
			}
		}

		// Send all commands accumulated from this frame to the audio thread
		m_commandQueue->Flush();

		Logger::ProcessLogs();
	}

	void AudioEngine::data_callback(ma_device* pDevice, void* pOutput, const void* pInput, uint32_t frameCount) {
		AudioEngine* engine = static_cast<AudioEngine*>(pDevice->pUserData);
		float* outputBuffer = static_cast<float*>(pOutput);

		engine->ProcessCommands();

		memset(outputBuffer, 0, frameCount * pDevice->playback.channels * sizeof(float));
		engine->Render(outputBuffer, frameCount);
	}

	void AudioEngine::ProcessCommands() {
		// We should probably set a limit on the amount of commands we process in one audio frame
		// Process AudioCommands
		AudioCommand cmd;
		while (m_commandQueue->Pop(cmd)) {
			switch (cmd.type) {
				case AudioCommand::Type::Play: {
					// TODO: Implement
				}
				case AudioCommand::Type::Pause: {
					// TODO: Implement
				}
				case AudioCommand::Type::Stop: {
					// TODO: Implement
				}
				default: {
					break;
				}
			}
		}
	}

	void AudioEngine::Render(float* outputBuffer, uint32_t frameCount) {
		// m_masterBus->Pull(outputBuffer, frameCount); should go here
	}
}

