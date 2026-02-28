#include "dalia/audio/AudioEngine.h"
#include "dalia/audio/AudioControl.h"
#include "core/Logger.h"
#include "core/FixedStack.h"
#include "core/SPSCRingBuffer.h"
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
		AudioEventCallback callback = nullptr;
		void* userData = nullptr;

		// We probably keep other voice data here too (volume etc.)
	};

	struct BusMirror {
		bool isBusy = false;
		uint32_t generation = 0;
		uint32_t parentBusIndex = NO_PARENT;

		// We probably keep other bus data here too (volume etc.)
	};

	struct ExecutionGraph {
		std::unique_ptr<uint32_t[]> listA;
		std::unique_ptr<uint32_t[]> listB;
	};

	struct EngineInternalState {
		bool initialized = false;

		// Miniaudio & Threads
		std::unique_ptr<ma_device> device;
		std::thread ioThread;
		std::atomic<bool> ioThreadRunning{false};

		// --- Messaging Queues ---
		std::unique_ptr<AudioCommandQueue>	audioCommandQueue;
		std::unique_ptr<AudioEventQueue>	audioEventQueue;
		std::unique_ptr<IoRequestQueue>		ioRequestQueue;

		// --- Resource Capacities ---
		uint32_t voiceCapacity	= 0;
		uint32_t streamCapacity	= 0;
		uint32_t busCapacity	= 0;

		// --- Pools (Audio Thread) ---
		std::unique_ptr<Voice[]>				voicePool;
		std::unique_ptr<StreamingContext[]>		streamPool;
		std::unique_ptr<Bus[]>					busPool;
		std::unique_ptr<float[]>				busMemoryPool;
		Bus* masterBus = nullptr;

		// --- Mirrors (Game Thread) ---
		std::unique_ptr<VoiceMirror[]>		voicePoolMirror;
		std::unique_ptr<BusMirror[]>		busPoolMirror;

		// -- Bus Execution Graph ---
		std::unique_ptr<ExecutionGraph>			busExecutionGraph;
		std::span<const uint32_t>				activeBusGraph;
		std::unique_ptr<uint32_t[]>				topologyScratchCount;
		std::unique_ptr<FixedStack<uint32_t>>	topologyLeavesStack;
		bool isAudioUsingGraphA = true;
		bool isBusGraphDirty = false;
		bool isGraphSwapPending = false;

		// --- Availability Containers ---
		std::unique_ptr<FixedStack<uint32_t>>		freeVoices;
		std::unique_ptr<SPSCRingBuffer<uint32_t>>	freeStreams;
		std::unique_ptr<FixedStack<uint32_t>>		freeBuses;

		EngineInternalState(const EngineConfig& config)
			: voiceCapacity(config.voiceCapacity), streamCapacity(config.streamCapacity), busCapacity(config.busCapacity) {
			// Message Queues
			audioCommandQueue	= std::make_unique<AudioCommandQueue>(config.commandQueueCapacity);
			audioEventQueue		= std::make_unique<AudioEventQueue>(config.eventQueueCapacity);
			ioRequestQueue		= std::make_unique<IoRequestQueue>(config.ioQueueCapacity);

			// Pools
			voicePool		= std::make_unique<Voice[]>(voiceCapacity);
			voicePoolMirror	= std::make_unique<VoiceMirror[]>(voiceCapacity);
			streamPool		= std::make_unique<StreamingContext[]>(streamCapacity);
			busPool			= std::make_unique<Bus[]>(busCapacity);
			busPoolMirror	= std::make_unique<BusMirror[]>(busCapacity);

			// Bus graph
			busExecutionGraph		= std::make_unique<ExecutionGraph>();
			topologyScratchCount	= std::make_unique<uint32_t[]>(busCapacity);
			topologyLeavesStack		= std::make_unique<FixedStack<uint32_t>>(busCapacity);

			// Availability Containers
			freeVoices	= std::make_unique<FixedStack<uint32_t>>(voiceCapacity);
			freeStreams = std::make_unique<SPSCRingBuffer<uint32_t>>(streamCapacity);
			freeBuses	= std::make_unique<FixedStack<uint32_t>>(busCapacity);
			for (uint32_t i = 0; i < voiceCapacity; i++)	freeVoices->Push(i);
			for (uint32_t i = 0; i < streamCapacity; i++)	freeStreams->Push(i);
			for (uint32_t i = 1; i < busCapacity; i++)		freeBuses->Push(i); // Skip Master (index 0)
		}


	};


	AudioEngine::AudioEngine() = default;
	AudioEngine::~AudioEngine() = default;

	Result AudioEngine::Init(const EngineConfig& config) {
		Logger::Init(config.logLevel, 256);

		if (m_state) {
			Logger::Log(LogLevel::Warning, "Engine", "Attempting to initialize engine that is already initialized");
			return Result::AlreadyInitialized;
		}

		m_state = std::make_unique<EngineInternalState>(config);

		// --- Device Setup ---
		m_state->device = std::make_unique<ma_device>();
		ma_device_config deviceConfig = ma_device_config_init(ma_device_type_playback);
		deviceConfig.playback.format = ma_format_f32;
		deviceConfig.playback.channels = 2; // Stereo playback only
		deviceConfig.sampleRate = 44100; // Hz
		deviceConfig.dataCallback = data_callback;
		deviceConfig.periodSizeInFrames = 480;
		deviceConfig.pUserData = this;

		if (ma_device_init(NULL, &deviceConfig, m_state->device.get()) != MA_SUCCESS) {
			Logger::Log(LogLevel::Critical, "Device", "Failed to initialize playback device");
			return Result::DeviceFailed;
		}
		const uint32_t m_periodSize = m_state->device->playback.internalPeriodSizeInFrames;
		Logger::Log(LogLevel::Info, "Engine", "Internal Period Size: %d", m_periodSize);

		// Bus buffer allocation and assignment
		// Setup bus memory pool (Room for 1024 frames) maybe we should check this against m_periodSize?
		const uint32_t samplesPerBus = 1024 * m_state->device->playback.channels;
		m_state->busMemoryPool		= std::make_unique<float[]>(m_state->busCapacity * samplesPerBus);

		// Bus buffer assigment
		for (uint32_t i = 0; i < m_state->busCapacity; i++) {
			float* busStart = &m_state->busMemoryPool[i * samplesPerBus];
			m_state->busPool[i].SetBuffer(std::span<float>(busStart, samplesPerBus));
		}
		m_state->masterBus = &m_state->busPool[0];
		m_state->masterBus->SetName("Master");

		// --- Audio Thread Start ---
		if (ma_device_start(m_state->device.get()) != MA_SUCCESS) {
			Logger::Log(LogLevel::Critical, "Device", "Failed to start playback device");
			return Result::DeviceFailed;
		}

		// --- I/O Thread Start ---
		m_state->ioThreadRunning.store(true, std::memory_order_relaxed);
		m_state->ioThread = std::thread(&AudioEngine::IoThreadMain, this);

		m_state->initialized = true;
		Logger::Log(LogLevel::Info, "Engine", "Initialized Audio Engine");
		return Result::Ok;
	}

	Result AudioEngine::Deinit() {
		if (!m_state) {
			Logger::Log(LogLevel::Warning, "Engine", "Attempting to deinitialize engine that is not initialized");
			return Result::NotInitialized;
		}

		// --- I/O Thread Stop ---
		m_state->ioThreadRunning.store(false, std::memory_order_release);
		if (m_state->ioThread.joinable()) m_state->ioThread.join();

		// --- Audio Thread Stop ---
		if (ma_device_stop(m_state->device.get()) != MA_SUCCESS) {
			Logger::Log(LogLevel::Critical, "Device", "Failed to stop playback device");
			return Result::DeviceFailed;
		}
		ma_device_uninit(m_state->device.get());

		m_state->initialized = false;
		Logger::Log(LogLevel::Info, "Engine", "Deinitialized Audio Engine");
		Logger::ProcessLogs(); // Drain the log buffer before shutdown
		return Result::Ok;
	}

	void AudioEngine::Update() {
		if (!m_state->initialized) return;

		// Process AudioEvents
		AudioEvent ev;
		while (m_state->audioEventQueue->Pop(ev)) {
			switch (ev.type) {
				case AudioEvent::Type::VoiceFinished: {
					// TODO: Implement
				}
				case AudioEvent::Type::PlaybackError: {
					// TODO: Implement
				}
				case AudioEvent::Type::GraphSwapped: {
					m_state->isAudioUsingGraphA = !m_state->isAudioUsingGraphA;
					m_state->isGraphSwapPending = false;
				}
			}
		}

		// Handle bus graph topology changes
		if (m_state->isBusGraphDirty && !m_state->isGraphSwapPending) {
			BuildBusGraph();
			m_state->isBusGraphDirty = false;
		}

		m_state->audioCommandQueue->Dispatch(); // Send all commands accumulated from this frame to the audio thread
		Logger::ProcessLogs(); // Print all logs accumulated from this frame
	}

	void AudioEngine::data_callback(ma_device* pDevice, void* pOutput, const void* pInput, uint32_t frameCount) {
		AudioEngine* engine = static_cast<AudioEngine*>(pDevice->pUserData);

		engine->ProcessCommands();

		engine->Render(static_cast<float*>(pOutput), frameCount);
	}

	void AudioEngine::IoThreadMain() {
		while (m_state->ioThreadRunning.load(std::memory_order_relaxed)) {
			bool didWork = false;
			IoRequest req;

			while (m_state->ioRequestQueue->Pop(req)) {
				didWork = true;

				// Handle requests
				switch (req.type) {
					case IoRequest::Type::RefillStreamBuffer: {
						StreamingContext& stream = m_state->streamPool[req.data.stream.poolIndex];

						// Is the stream still valid?
						if (stream.generation.load(std::memory_order_relaxed) != req.data.stream.generation) {
							break;
						}

						// TODO: Read data into buffer from soundbank
						// Remember that if read reaches EOF, it should mark it in the buffer and keep reading
						// from the start of the file to fill the entire buffer.
					}
					case IoRequest::Type::ReleaseStream: {
						StreamingContext& stream = m_state->streamPool[req.data.stream.poolIndex];
						// TODO: Release decoder here (if StreamingContext has one)
						m_state->freeStreams->Push(req.data.stream.poolIndex);
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
		while (m_state->audioCommandQueue->Pop(cmd)) {
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
				case AudioCommand::Type::SwapGraph: {
					if (cmd.data.graph.useListA) {
						m_state->activeBusGraph = std::span<const uint32_t>(
							m_state->busExecutionGraph->listA.get(),
							cmd.data.graph.nodeCount
						);
					}
					else {
						m_state->activeBusGraph = std::span<const uint32_t>(
						m_state->busExecutionGraph->listB.get(),
						cmd.data.graph.nodeCount
						);
					}

					// Send event to acknowledge the swap
					m_state->audioEventQueue->Push(AudioEvent::GraphSwapped());
				}
				default: {
					break;
				}
			}
		}
	}

	void AudioEngine::Render(float* output, uint32_t frameCount) {
		const uint32_t sampleCount = frameCount * m_state->device->playback.channels;

		// Clear all buses in the graph
		for (uint32_t busIndex : m_state->activeBusGraph) {
			m_state->busPool[busIndex].Clear();
		}

		// --- Voice Pass --- (Parallel ready when the time comes)
		for (uint32_t i = 0; i < m_state->voiceCapacity; i++) {
			Voice& voice = m_state->voicePool[i];
			if (!voice.isPlaying) continue;

			bool isStillPlaying = MixVoiceToBus(voice, voice.parentBusIndex, frameCount);
			if (!isStillPlaying) {
				// Voice finished
				m_state->audioEventQueue->Push(AudioEvent::VoiceFinished(i));
				voice.Reset();
			}
		}

		// --- Bus Pass --- (Not yet parallel ready)
		for (uint32_t busIndex : m_state->activeBusGraph) {
			Bus& bus = m_state->busPool[busIndex];
			bus.ApplyDSP(sampleCount);

			// Maybe we just replace this with a bus->MixToParent(uint32_t sampleCount)?
			uint32_t parentIndex = bus.parentIndex;
			if (parentIndex != NO_PARENT) {
				Bus& parentBus = m_state->busPool[parentIndex];
				parentBus.MixInBuffer(bus.GetBuffer(), sampleCount);
			}
		}

		// Final Output (clamped between -1.0f and 1.0f
		auto masterBuffer = m_state->masterBus->GetBuffer();
		for (uint32_t i = 0; i < sampleCount; i++) {
			masterBuffer[i] = std::clamp(masterBuffer[i], -1.0f, 1.0f);
		}
		std::copy_n(masterBuffer.data(), sampleCount, output);
	}

	bool AudioEngine::MixVoiceToBus(Voice& voice, uint32_t busIndex, uint32_t frameCount) {
		std::span<float> busBuffer = m_state->busPool[busIndex].GetBuffer();
		uint32_t framesMixed = 0;

		while (framesMixed < frameCount) {
			uint32_t framesInSource = 0;
			float* sourceData = nullptr;

			if (voice.sourceType == VoiceSourceType::Resident) {
				sourceData = voice.buffer.data();
				framesInSource = static_cast<uint32_t>(voice.buffer.size() / voice.channels);
			}
			else if (voice.sourceType == VoiceSourceType::Stream) {
				StreamingContext& stream = m_state->streamPool[voice.streamingContextIndex];
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

			// --- Panning & DSP ---
			const float panNormalized = (voice.pan + 1.0f) * 0.5f;
			const float angle = panNormalized * static_cast<float>(M_PI_2); // Angle between 0 and PI/2 radians
			const float gainL = std::cos(angle) * voice.volume;
			const float gainR = std::sin(angle) * voice.volume;

			for (uint32_t i = 0; i < framesToProcessNow; i++) {
				float sample = sourceData[static_cast<size_t>(voice.cursor) * voice.channels];
				uint32_t targetIndex = (framesMixed + i) * 2; // 2 Channels
				busBuffer[targetIndex + 0] += sample * gainL;
				busBuffer[targetIndex + 1] += sample * gainR;

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
					StreamingContext& stream = m_state->streamPool[voice.streamingContextIndex];

					if (framesInSource < DOUBLE_BUFFER_CHUNK_SIZE) {
						// Natural end of file -> Kill voice
						// TODO: The two rows below should probably be part of some kind of releaseVoice function later on
						stream.generation.fetch_add(1, std::memory_order_relaxed);
						m_state->ioRequestQueue->Push(IoRequest::ReleaseStream(voice.streamingContextIndex));

						return false;
					}

					stream.bufferReady[stream.frontBufferIndex].store(false, std::memory_order_relaxed);
					const uint32_t gen = stream.generation.load(std::memory_order_relaxed);
					m_state->ioRequestQueue->Push(IoRequest::RefillStreamBuffer(voice.streamingContextIndex, gen, stream.frontBufferIndex));

					// Swap buffers
					stream.frontBufferIndex = 1 - stream.frontBufferIndex;
					voice.cursor = 0.0f;

					if (!stream.bufferReady[stream.frontBufferIndex].load(std::memory_order_acquire)) {
						Logger::Log(LogLevel::Critical, "Voice (Streaming) Mixer", "Buffer underflow. Killing Voice...");

						// TODO: The two rows below should probably be part of some kind of releaseVoice function later on
						stream.generation.fetch_add(1, std::memory_order_relaxed);
						m_state->ioRequestQueue->Push(IoRequest::ReleaseStream(voice.streamingContextIndex));

						return false;
					}
				}
				else return false;
			}
		}

		return true;
	}

	void AudioEngine::BuildBusGraph() {
		bool writingToListA = !m_state->isAudioUsingGraphA;
		auto& targetList = writingToListA ? m_state->busExecutionGraph->listA : m_state->busExecutionGraph->listB;
		uint32_t sortedCount = 0;
		uint32_t activeBusCount = 0; // For cycle detection

		// Clear pre-allocated containers
		std::fill_n(m_state->topologyScratchCount.get(), m_state->busCapacity, 0);
		m_state->topologyLeavesStack->Clear();

		// Calculate the amount of children each bus has
		for (uint32_t i = 0; i < m_state->busCapacity; i++) {
			if (m_state->busPoolMirror[i].isBusy) {
				activeBusCount++;
				uint32_t parent = m_state->busPoolMirror[i].parentBusIndex;
				if (parent != NO_PARENT) {
					m_state->topologyScratchCount[parent]++;
				}
			}
		}

		// Find the buses with no children (leaves)
		for (uint32_t i = 0; i < m_state->busCapacity; i++) {
			if (m_state->busPoolMirror[i].isBusy && m_state->topologyScratchCount[i] == 0) {
				m_state->topologyLeavesStack->Push(i);
			}
		}

		// Kahn's algorithm (process from leaves to root)
		while (!m_state->topologyLeavesStack->IsEmpty()) {
			uint32_t currentBus;
			m_state->topologyLeavesStack->Pop(currentBus);

			// Add it to execution list
			targetList[sortedCount] = currentBus;
			sortedCount++;

			// Tell the parent that one of its children is sorted
			uint32_t parent = m_state->busPoolMirror[currentBus].parentBusIndex;
			if (parent != NO_PARENT) {
				m_state->topologyScratchCount[parent]--;

				// If the parent has no more pending children, it is ready to be processed
				if (m_state->topologyScratchCount[parent] == 0) {
					m_state->topologyLeavesStack->Push(parent);
				}
			}
		}

		// This should never happen!
		if (sortedCount != activeBusCount) {
			Logger::Log(LogLevel::Critical, "Bus Graph", "Cycle detected in bus graph. Graph update aborted.");
			return;
		}

		// Send command to swap list
		m_state->audioCommandQueue->Enqueue(AudioCommand::SwapGraph(writingToListA, sortedCount));
		m_state->isGraphSwapPending = true;
	}
}

