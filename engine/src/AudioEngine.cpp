#include "dalia/AudioEngine.h"
#include "Logger.h"
#include "AudioCommandQueue.h"
#include "AudioEventQueue.h"
#include "IoRequestQueue.h"
#include "Voice.h"
#include "StreamingContext.h"
#include "Bus.h"

#include <algorithm>
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
		const uint32_t m_periodSize = m_device->playback.internalPeriodSizeInFrames;
		Logger::Log(LogLevel::Info, "Engine", "Internal Period Size: %d", m_periodSize);

		// --- Setup Queues ---
		m_audioCommandQueue = std::make_unique<AudioCommandQueue>(config.commandQueueCapacity);
		m_audioEventQueue = std::make_unique<AudioEventQueue>(config.eventQueueCapacity);
		m_ioRequestQueue = std::make_unique<IoRequestQueue>(config.ioQueueCapacity);

		// --- Setup Voices & Buses ---
		m_voiceCapacity = config.voiceCapacity;
		m_streamCapacity = config.streamCapacity;
		m_busCapacity = config.busCapacity;

		m_voicePool = std::make_unique<Voice[]>(m_voiceCapacity);
		m_streamPool = std::make_unique<StreamingContext[]>(m_streamCapacity);
		m_busPool = std::make_unique<Bus[]>(m_busCapacity);

		// Setup bus memory pool (Room for 1024 frames) maybe we should check this against m_periodSize?
		const uint32_t samplesPerBus = 1024 * m_device->playback.channels;
		m_busMemoryPool = std::make_unique<float[]>(m_busCapacity * samplesPerBus);

		// Master bus initialization
		m_masterBus = &m_busPool[0];
		m_masterBus->SetName("Master");
		// Bus initialization
		for (uint32_t i = 1; i < m_busCapacity; i++) {
			float* busStart = &m_busMemoryPool[i * samplesPerBus];
			m_busPool[i].SetBuffer(std::span<float>(busStart, samplesPerBus));
			m_busPool[i].SetName("Bus_" + std::to_string(i));
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
		while (m_audioEventQueue->Pop(ev)) {
			switch (ev.type) {
				case AudioEvent::Type::VoiceFinished: {
					// TODO: Implement
				}
				case AudioEvent::Type::PlaybackError: {
					// TODO: Implement
				}
			}
		}

		// Send all commands accumulated from this frame to the audio thread
		m_audioCommandQueue->Dispatch();

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
		while (m_audioCommandQueue->Pop(cmd)) {
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
		// Clear all bus buffers
		for (uint32_t i = 0; i < m_busCapacity; i++) {
			Bus& bus = m_busPool[i];
			bus.Clear();
		}

		// Mix all voices into their parent bus
		for (uint32_t i = 0; i < m_voiceCapacity; i++) {
			Voice& voice = m_voicePool[i];
			if (!voice.isPlaying) {
				// If voice has finished playing, handle that here
				continue;
			}

			bool busy = MixVoiceToBus(voice, voice.parentBus, frameCount);
			// set busy value in game thread voice slot array
		}

		// Create a std::span from output buffer and clear it
		std::span<float> outputBuffer = std::span<float>(output, frameCount * m_device->playback.channels);
		std::fill(outputBuffer.begin(), outputBuffer.end(), 0.0f);

		// Recursive audio graph pull
		const uint32_t sampleCount = frameCount * m_device->playback.channels;
		m_masterBus->Pull(outputBuffer, sampleCount);
	}

	bool AudioEngine::MixVoiceToBus(Voice& voice, Bus* bus, uint32_t frameCount) {
		float* busBufferData = bus->getBuffer().data();
		uint32_t framesMixed = 0;

		while (framesMixed < frameCount) {
			uint32_t framesInSource = 0;
			float* sourceData = nullptr;

			if (voice.sourceType == VoiceSourceType::Resident) {
				sourceData = voice.buffer.data();
				framesInSource = static_cast<uint32_t>(voice.buffer.size() / voice.channels);
			}
			else {
				// Stream
				StreamingContext* stream = voice.streamingContext;
				sourceData = stream->buffers[stream->frontBufferIndex];
				framesInSource = DOUBLE_BUFFER_CHUNK_SIZE;
			}

			uint32_t remainingInSource = framesInSource - static_cast<uint32_t>(voice.cursor);
			uint32_t framesToProcessNow = std::min(frameCount - framesMixed, remainingInSource);

			// Apply pan and volume
			const float panNormalized = (voice.pan + 1.0f) * 0.5f;
			const float angle = panNormalized * static_cast<float>(M_PI_2); // Angle between 0 and PI/2 radians
			const float gainL = std::cos(angle) * voice.volume;
			const float gainR = std::sin(angle) * voice.volume;

			for (uint32_t i = 0; i < framesToProcessNow; i++) {
				float sample = sourceData[static_cast<size_t>(voice.cursor) * voice.channels];
				uint32_t busIndex = (framesMixed + i) * 2; // 2 Channels
				busBufferData[busIndex + 0] += sample * gainL;
				busBufferData[busIndex + 1] += sample * gainR;

				voice.cursor += 1.0f;
			}
			framesMixed += framesToProcessNow;

			// Handle end of buffer
			if (static_cast<uint32_t>(voice.cursor) >= framesInSource) {
				if (voice.sourceType == VoiceSourceType::Resident) {
					if (voice.isLooping) {
						voice.cursor = 0.0f;
					}
					else {
						// Voice is no longer used -> slot should be free now (return false?)
						voice.isPlaying = false;
						return false;
					}
				}
				else {
					// Stream -> Swap buffer
					StreamingContext* stream = voice.streamingContext;
					stream->bufferReady[stream->frontBufferIndex] = false;

					stream->frontBufferIndex = 1 - stream->frontBufferIndex;
					voice.cursor = 0.0f;

					if (!stream->bufferReady[stream->frontBufferIndex]) {
						// We probably return false here
						voice.isPlaying = false;
						return false;
					}
				}
			}
		}

		return true;
	}
}

