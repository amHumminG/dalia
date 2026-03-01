#include "dalia/audio/Engine.h"
#include "dalia/audio/SoundControl.h"
#include "core/Logger.h"
#include "core/FixedStack.h"
#include "core/SPSCRingBuffer.h"
#include "messaging/RtCommandQueue.h"
#include "messaging/RtEventQueue.h"
#include "messaging/IoRequestQueue.h"
#include "mixer/Voice.h"
#include "mixer/StreamingContext.h"
#include "mixer/Bus.h"

#include "systems/RealTimeSystem.h"
#include "systems/IoSystem.h"

#include <algorithm>
#include <span>

#include "miniaudio.h"

#ifndef M_PI_2
#define M_PI_2 1.57079632679489661923
#endif

namespace dalia {

	static constexpr uint8_t CHANNELS_STEREO = 2;

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

		// Miniaudio
		std::unique_ptr<ma_device> device;

		// --- Messaging Queues ---
		std::unique_ptr<RtCommandQueue>		rtCommandQueue;
		std::unique_ptr<RtEventQueue>		rtEventQueue;
		std::unique_ptr<IoRequestQueue>		ioRequestQueue;

		// --- Resource Capacities ---
		uint32_t voiceCapacity	= 0;
		uint32_t streamCapacity	= 0;
		uint32_t busCapacity	= 0;

		// --- Pools ---
		std::unique_ptr<Voice[]>				voicePool;
		std::unique_ptr<StreamingContext[]>		streamPool;
		std::unique_ptr<Bus[]>					busPool;
		std::unique_ptr<float[]>				busMemoryPool;
		Bus* masterBus = nullptr;

		// --- Mirrors ---
		std::unique_ptr<VoiceMirror[]>		voicePoolMirror;
		std::unique_ptr<BusMirror[]>		busPoolMirror;

		// --- Availability Containers ---
		std::unique_ptr<FixedStack<uint32_t>>		freeVoices;
		std::unique_ptr<SPSCRingBuffer<uint32_t>>	freeStreams;
		std::unique_ptr<FixedStack<uint32_t>>		freeBuses;

		// -- Bus Execution Graph ---
		std::unique_ptr<ExecutionGraph>			busExecutionGraph;
		std::span<const uint32_t>				activeBusGraph;
		std::unique_ptr<uint32_t[]>				topologyScratchCount;
		std::unique_ptr<FixedStack<uint32_t>>	topologyLeavesStack;
		bool isAudioUsingGraphA = true;
		bool isBusGraphDirty = false;
		bool isGraphSwapPending = false;

		std::unique_ptr<RealTimeSystem> realTimeSystem;
		std::unique_ptr<IoSystem> ioSystem;

		EngineInternalState(const EngineConfig& config)
			: voiceCapacity(config.voiceCapacity), streamCapacity(config.streamCapacity), busCapacity(config.busCapacity) {
			// Message Queues
			rtCommandQueue		= std::make_unique<RtCommandQueue>(config.rtCommandQueueCapacity);
			rtEventQueue		= std::make_unique<RtEventQueue>(config.rtEventQueueCapacity);
			ioRequestQueue		= std::make_unique<IoRequestQueue>(config.ioRequestQueueCapacity);

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


	Engine::Engine() = default;
	Engine::~Engine() = default;

	Result Engine::Init(const EngineConfig& config) {
		Logger::Init(config.logLevel, 256);

		if (m_state) {
			Logger::Log(LogLevel::Warning, "Engine", "Attempting to initialize engine that is already initialized");
			return Result::AlreadyInitialized;
		}

		// --- INTERNAL STATE SETUP ---
		m_state = std::make_unique<EngineInternalState>(config);

		// Bus buffer allocation and assignment
		// Setup bus memory pool (Room for 1024 frames) maybe we should check this against m_periodSize?
		const uint32_t samplesPerBus = 1024 * CHANNELS_STEREO;
		m_state->busMemoryPool		= std::make_unique<float[]>(m_state->busCapacity * samplesPerBus);

		// Bus buffer assigment
		for (uint32_t i = 0; i < m_state->busCapacity; i++) {
			float* busStart = &m_state->busMemoryPool[i * samplesPerBus];
			m_state->busPool[i].SetBuffer(std::span<float>(busStart, samplesPerBus));
		}

		m_state->masterBus = &m_state->busPool[0];
		m_state->masterBus->SetName("Master");

		// --- SYSTEMS SETUP ---
		RealTimeSystemConfig rtConfig;
		rtConfig.rtCommandQueue = m_state->rtCommandQueue.get();
		rtConfig.rtEventQueue	= m_state->rtEventQueue.get();
		rtConfig.ioRequestQueue = m_state->ioRequestQueue.get();
		rtConfig.voicePool		= std::span<Voice>(m_state->voicePool.get(), m_state->voiceCapacity);
		rtConfig.streamPool		= std::span<StreamingContext>(m_state->streamPool.get(), m_state->streamCapacity);
		rtConfig.busPool		= std::span<Bus>(m_state->busPool.get(), m_state->busCapacity);
		rtConfig.masterBus		= m_state->masterBus;
		m_state->realTimeSystem = std::make_unique<RealTimeSystem>(rtConfig);

		IoSystemConfig ioConfig;
		ioConfig.ioRequestQueue = m_state->ioRequestQueue.get();
		ioConfig.streamPool		= std::span<StreamingContext>(m_state->streamPool.get(), m_state->streamCapacity);
		ioConfig.freeStreams	= m_state->freeStreams.get();
		m_state->ioSystem		= std::make_unique<IoSystem>(ioConfig);

		// --- BACKEND SETUP ---
		m_state->device					= std::make_unique<ma_device>();
		ma_device_config deviceConfig	= ma_device_config_init(ma_device_type_playback);
		deviceConfig.playback.format	= ma_format_f32;
		deviceConfig.playback.channels	= CHANNELS_STEREO; // Stereo playback only
		deviceConfig.sampleRate			= 44100; // Hz
		deviceConfig.dataCallback		= data_callback;
		deviceConfig.periodSizeInFrames = 480;
		deviceConfig.pUserData			= m_state->realTimeSystem.get();

		if (ma_device_init(NULL, &deviceConfig, m_state->device.get()) != MA_SUCCESS) {
			Logger::Log(LogLevel::Critical, "Device", "Failed to initialize playback device");
			return Result::DeviceFailed;
		}
		// const uint32_t m_periodSize = m_state->device->playback.internalPeriodSizeInFrames;
		// Logger::Log(LogLevel::Info, "Engine", "Internal Period Size: %d", m_periodSize);


		// --- SYSTEMS START ---
		// I/O Thread Start
		m_state->ioSystem->Start();

		// Audio Thread Start
		if (ma_device_start(m_state->device.get()) != MA_SUCCESS) {
			Logger::Log(LogLevel::Critical, "Device", "Failed to start playback device");
			return Result::DeviceFailed;
		}

		m_state->initialized = true;
		Logger::Log(LogLevel::Info, "Engine", "Initialized engine");
		return Result::Ok;
	}

	Result Engine::Deinit() {
		if (!m_state) {
			Logger::Log(LogLevel::Warning, "Engine", "Attempting to deinitialize engine that is not initialized");
			return Result::NotInitialized;
		}

		// --- Audio Thread Stop ---
		if (ma_device_stop(m_state->device.get()) != MA_SUCCESS) {
			Logger::Log(LogLevel::Critical, "Device", "Failed to stop playback device");
			return Result::DeviceFailed;
		}
		ma_device_uninit(m_state->device.get());

		// --- I/O Thread Stop ---
		m_state->ioSystem->Stop();

		m_state->initialized = false;
		Logger::Log(LogLevel::Info, "Engine", "Deinitialized engine");
		Logger::ProcessLogs(); // Drain the log buffer before shutdown
		return Result::Ok;
	}

	void Engine::Update() {
		if (!m_state->initialized) return;

		// Process AudioEvents
		RtEvent ev;
		while (m_state->rtEventQueue->Pop(ev)) {
			switch (ev.type) {
				case RtEvent::Type::VoiceFinished: {
					// TODO: Implement
					// Trigger callback
				}
				case RtEvent::Type::PlaybackError: {
					// TODO: Implement
				}
				case RtEvent::Type::GraphSwapped: {
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

		m_state->rtCommandQueue->Dispatch(); // Send all commands accumulated from this frame to the audio thread
		Logger::ProcessLogs(); // Print all logs accumulated from this frame
	}

	void Engine::data_callback(ma_device* pDevice, void* pOutput, const void* pInput, uint32_t frameCount) {
		RealTimeSystem* rtSystem = static_cast<RealTimeSystem*>(pDevice->pUserData);
		rtSystem->OnAudioCallback(static_cast<float*>(pOutput), frameCount, pDevice->playback.channels);
	}

	void Engine::BuildBusGraph() {
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

		// Send command to swap
		uint32_t* listPtr = writingToListA ? m_state->busExecutionGraph->listA.get() : m_state->busExecutionGraph->listB.get();
		m_state->rtCommandQueue->Enqueue(RtCommand::SwapGraph(listPtr, sortedCount));
		m_state->isGraphSwapPending = true;
	}
}