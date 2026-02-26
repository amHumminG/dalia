#include "dalia/audio/AudioEngine.h"
#include "dalia/audio/AudioControl.h"
#include "core/Logger.h"
#include "messaging/AudioCommandQueue.h"
#include "messaging/AudioEventQueue.h"
#include "messaging/IoRequestQueue.h"
#include "mixer/Voice.h"
#include "mixer/StreamingContext.h"
#include "mixer/Bus.h"

#include <thread>
#include <algorithm>
#include <span>
#include <cmath>
#include <chrono>

#include "miniaudio.h"

#ifndef M_PI_2
#define M_PI_2 1.57079632679489661923
#endif

namespace dalia {

	struct VoiceMirror {
		bool isBusy = false;
		uint32_t generation = 0;
		void* callbackOnFinished = nullptr;
		void* userData = nullptr;
	};

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
		for (uint32_t i = 0; i < m_streamCapacity; i++) m_freeStreamQueue->Push(i);
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

		// --- I/O Thread Start ---
		m_ioThreadRunning.store(true, std::memory_order_relaxed);
		m_ioThread = std::thread(&AudioEngine::IoThreadMain, this);

		// --- Audio Thread Start ---
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

		// --- I/O Thread Stop ---
		m_ioThreadRunning.store(false, std::memory_order_release);
		if (m_ioThread.joinable()) m_ioThread.join();

		// --- Audio Thread Stop ---
		if (ma_device_stop(m_device.get()) != MA_SUCCESS) {
			Logger::Log(LogLevel::Critical, "Device", "Failed to stop playback device");
			return Result::DeviceFailed;
		}
		ma_device_uninit(m_device.get());

		m_initialized = false;
		Logger::Log(LogLevel::Info, "Engine", "Deinitialized Audio Engine");
		Logger::ProcessLogs(); // Drain the log buffer before shutdown
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

	void AudioEngine::IoThreadMain() {
		while (m_ioThreadRunning.load(std::memory_order_relaxed)) {
			bool didWork = false;
			IoRequest req;

			while (m_ioRequestQueue->Pop(req)) {
				didWork = true;

				// Handle requests
				switch (req.type) {
					case IoRequest::Type::RefillStreamBuffer: {
						StreamingContext& stream = m_streamPool[req.data.stream.poolIndex];

						// Is the stream still valid?
						if (stream.generation.load(std::memory_order_relaxed) != req.data.stream.generation) {
							break;
						}

						// TODO: Read data into buffer from soundbank
						// Remember that if read reaches EOF, it should mark it in the buffer and keep reading
						// from the start of the file to fill the entire buffer.
					}
					case IoRequest::Type::ReleaseStream: {
						StreamingContext& stream = m_streamPool[req.data.stream.poolIndex];
						// TODO: Release decoder here (if StreamingContext has one)
						m_freeStreamQueue->Push(req.data.stream.poolIndex);
						break;
					}
					case IoRequest::Type::LoadSoundbank: {
						// TODO: Logic for soundbank loading
						break;
					}
					default:
						break;
				}
			}

			if (!didWork) {
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
		}
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
		auto outputBuffer = std::span<float>(output, frameCount * m_device->playback.channels);
		std::ranges::fill(outputBuffer, 0.0f);

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
			else if (voice.sourceType == VoiceSourceType::Stream) {
				StreamingContext& stream = m_streamPool[voice.streamingContextIndex];
				sourceData = stream.buffers[stream.frontBufferIndex];

				// Check if we hit EOF this buffer and on what index
				const uint32_t eofIndex = stream.eofIndex[stream.frontBufferIndex];
				if (!voice.isLooping && eofIndex != NO_EOF) {
					framesInSource = eofIndex;
				}
				else {
					framesInSource = DOUBLE_BUFFER_CHUNK_SIZE;
				}
			}
			else return false;

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
				else if (voice.sourceType == VoiceSourceType::Stream) {
					StreamingContext& stream = m_streamPool[voice.streamingContextIndex];

					if (framesInSource < DOUBLE_BUFFER_CHUNK_SIZE) {
						// Natural end of file -> Kill voice
						// TODO: The two rows below should probably be part of some kind of releaseVoice function later on
						stream.generation.fetch_add(1, std::memory_order_relaxed);
						m_ioRequestQueue->Push(IoRequest::ReleaseStream(voice.streamingContextIndex));

						return false;
					}

					stream.bufferReady[stream.frontBufferIndex].store(false, std::memory_order_relaxed);
					const uint32_t gen = stream.generation.load(std::memory_order_relaxed);
					m_ioRequestQueue->Push(IoRequest::RefillStreamBuffer(voice.streamingContextIndex, gen, stream.frontBufferIndex));

					// Swap buffers
					stream.frontBufferIndex = 1 - stream.frontBufferIndex;
					voice.cursor = 0.0f;

					if (!stream.bufferReady[stream.frontBufferIndex].load(std::memory_order_acquire)) {
						Logger::Log(LogLevel::Critical, "Voice (Streaming) Mixer", "Buffer underflow. Killing Voice...");

						// TODO: The two rows below should probably be part of some kind of releaseVoice function later on
						stream.generation.fetch_add(1, std::memory_order_relaxed);
						m_ioRequestQueue->Push(IoRequest::ReleaseStream(voice.streamingContextIndex));

						return false;
					}
				}
				else return false;
			}
		}

		return true;
	}
}

