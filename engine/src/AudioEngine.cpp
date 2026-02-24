#include "dalia/AudioEngine.h"
#include "Logger.h"
#include "CommandQueue.h"
#include "EventQueue.h"
#include "Voice.h"
#include "StreamingContext.h"
#include "Bus.h"

#include <span>
#include <cmath>

#include "miniaudio.h"

#ifndef M_PI_2
#define M_PI_2 1.57079632679489661923
#endif

namespace dalia {

	AudioEngine::AudioEngine() = default;

	AudioEngine::~AudioEngine() {
		// Any manual deallocations here
	}

	Result AudioEngine::Init(const EngineConfig& config) {
		Logger::Init(config.logLevel, 256);

		if (m_initialized) {
			Logger::Log(LogLevel::Warning, "Engine", "Attempting to initialize engine that is already initialized");
			return Result::AlreadyInitialized;
		}

		// --- Device Setup ---
		m_device = std::make_unique<ma_device>();
		ma_device_config deviceConfig = ma_device_config_init(ma_device_type_playback);
		deviceConfig.playback.format = ma_format_f32;
		deviceConfig.playback.channels = 2; // Stereo playback only
		deviceConfig.sampleRate = 44100; // Hz
		deviceConfig.dataCallback = data_callback;
		deviceConfig.periodSizeInFrames = 480;
		deviceConfig.pUserData = this;

		if (ma_device_init(NULL, &deviceConfig, m_device.get()) != MA_SUCCESS) {
			Logger::Log(LogLevel::Critical, "Device", "Failed to initialize playback device");
			return Result::DeviceFailed;
		}
		m_periodSize = m_device->playback.internalPeriodSizeInFrames;
		Logger::Log(LogLevel::Info, "Engine", "Internal Period Size: %d", m_periodSize);

		// --- Setup Queues ---
		m_commandQueue = std::make_unique<CommandQueue>(config.commandBufferCapacity);
		m_eventQueue = std::make_unique<EventQueue>(config.eventBufferCapacity);

		// --- Setup Buses ---
		m_voiceCapacity = config.voiceCapacity;
		m_busCapacity = config.busCapacity;
		m_voices = std::make_unique<Voice[]>(m_voiceCapacity);
		m_buses = std::make_unique<Bus[]>(m_busCapacity);

		// Setup bus memory pool (Room for 1024 frames) maybe we should check this against m_periodSize?
		const uint32_t samplesPerBus = 1024 * m_device->playback.channels;
		m_busMemoryPool = std::make_unique<float[]>(m_busCapacity * samplesPerBus);
		// Master bus initialization
		m_masterBus = &m_buses[0];
		m_masterBus->SetName("Master");
		// Bus initialization
		for (uint32_t i = 1; i < m_busCapacity; i++) {
			float* busStart = &m_busMemoryPool[i * samplesPerBus];
			m_buses[i].SetBuffer(std::span<float>(busStart, samplesPerBus));
			m_buses[i].SetName("Bus_" + std::to_string(i));
		}

		// --- Device Start --- (Audio thread starts here)
		if (ma_device_start(m_device.get()) != MA_SUCCESS) {
			Logger::Log(LogLevel::Critical, "Device", "Failed to start playback device");
			return Result::DeviceFailed;
		}

		m_initialized = true;
		Logger::Log(LogLevel::Info, "Engine", "Initialized Audio Engine");
		return Result::Ok;
	}

	Result AudioEngine::Deinit() {
		if (!m_initialized) {
			Logger::Log(LogLevel::Warning, "Engine", "Attempting to deinitialize engine that is not initialized");
			return Result::NotInitialized;
		}

		// --- Device Stop & Teardown ---
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
		if (!m_initialized) return;

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

		engine->ProcessCommands();

		engine->Render(static_cast<float*>(pOutput), frameCount);
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

	void AudioEngine::Render(float* output, uint32_t frameCount) {
		std::span<float> outputBuffer = std::span<float>(output, frameCount * m_device->playback.channels);
		std::fill(outputBuffer.begin(), outputBuffer.end(), 0.0f);

		// Clear all bus buffers
		for (auto& bus : m_buses) {
			bus.clear();
		}

		// Mix all voices into their parent bus
		for (auto& voice : m_voices) {
			if (!voice.isPlaying) {
				// If voice has finished playing, handle that here
				continue;
			}

			MixVoiceToBus(voice, voice.parentBus, frameCount);
		}

		// Recursive audio graph pull
		m_masterBus.Pull(outputBuffer, sampleCount);
	}

	void AudioEngine::MixVoiceToBus(Voice& voice, Bus* bus, uint32_t frameCount) {
		float* busBufferData = bus->getBuffer().data();
		float* voiceBufferData = voice.buffer.data();

		// Mixing happens here
		for (uint32_t f = 0; f < frameCount; f++) {
			float sample = voiceBufferData[static_cast<size_t>(voice.cursor)];

			// Apply pan and volume
			float panNormalized = (voice.pan + 1.0f) * 0.5f;
			float angle = panNormalized * static_cast<float>(M_PI_2); // Angle between 0 and PI/2 radians

			float gainL = std::cos(angle) * voice.volume;
			float gainR = std::sin(angle) * voice.volume;

			busBufferData[f * voice.channels + 0] += sample * gainL;
			busBufferData[f * voice.channels + 1] += sample * gainR;

			voice.cursor += 1; // Cursor moves one frame forward
		}

		// This is where we check if a stream buffer needs to be refilled
	}
}

