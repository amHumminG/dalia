#include "dalia/audio/Engine.h"
#include "dalia/audio/PlaybackControl.h"
#include "core/Logger.h"
#include "core/FixedStack.h"
#include "core/SPSCRingBuffer.h"
#include "messaging/RtCommandQueue.h"
#include "messaging/RtEventQueue.h"
#include "messaging/IoStreamRequestQueue.h"
#include "mixer/Voice.h"
#include "mixer/StreamContext.h"
#include "mixer/Bus.h"
#include "mixer/BusGraphCompiler.h"

#include "systems/RtSystem.h"
#include "systems/IoStreamSystem.h"

#include <span>

#include "miniaudio.h"

namespace dalia {

	static constexpr uint8_t CHANNELS_STEREO = 2;
	static constexpr uint32_t MASTER_BUS_INDEX = 0;

	struct MixOrderBuffer {
		std::unique_ptr<uint32_t[]> listA;
		std::unique_ptr<uint32_t[]> listB;

		explicit MixOrderBuffer(uint32_t capacity) {
			listA = std::make_unique<uint32_t[]>(capacity);
			listB = std::make_unique<uint32_t[]>(capacity);
		}
	};

	struct EngineInternalState {
		bool initialized = false;

		// Miniaudio
		std::unique_ptr<ma_device> device;

		// --- Messaging Queues ---
		std::unique_ptr<RtCommandQueue>			rtCommands;
		std::unique_ptr<RtEventQueue>			rtEvents;
		std::unique_ptr<IoStreamRequestQueue>	ioStreamRequests;

		// --- Resource Capacities ---
		uint32_t voiceCapacity	= 0;
		uint32_t streamCapacity	= 0;
		uint32_t busCapacity	= 0;

		// --- Pools ---
		std::unique_ptr<Voice[]>				voicePool;
		std::unique_ptr<StreamContext[]>		streamPool;
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
		std::unique_ptr<MixOrderBuffer>		mixOrderBuffer;
		std::unique_ptr<BusGraphCompiler>	busGraphCompiler;

		bool isUsingMixOrderA		= true;
		bool isMixOrderDirty		= false;
		bool isMixOrderSwapPending	= false;

		std::unique_ptr<RtSystem> rtSystem;
		std::unique_ptr<IoStreamSystem> ioStreamSystem;

		EngineInternalState(const EngineConfig& config)
			: voiceCapacity(config.voiceCapacity), streamCapacity(config.streamCapacity), busCapacity(config.busCapacity) {
			// Message Queues
			rtCommands			= std::make_unique<RtCommandQueue>(config.rtCommandQueueCapacity);
			rtEvents			= std::make_unique<RtEventQueue>(config.rtEventQueueCapacity);
			ioStreamRequests	= std::make_unique<IoStreamRequestQueue>(config.ioStreamRequestQueueCapacity);

			// Pools
			voicePool		= std::make_unique<Voice[]>(voiceCapacity);
			voicePoolMirror	= std::make_unique<VoiceMirror[]>(voiceCapacity);
			streamPool		= std::make_unique<StreamContext[]>(streamCapacity);
			busPool			= std::make_unique<Bus[]>(busCapacity);
			busPoolMirror	= std::make_unique<BusMirror[]>(busCapacity);

			// Bus graph
			mixOrderBuffer		= std::make_unique<MixOrderBuffer>(busCapacity);
			busGraphCompiler	= std::make_unique<BusGraphCompiler>(busCapacity);

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
		constexpr uint32_t samplesPerBus = 1024 * CHANNELS_STEREO;
		m_state->busMemoryPool = std::make_unique<float[]>(m_state->busCapacity * samplesPerBus);

		// Bus buffer assigment
		for (uint32_t i = 0; i < m_state->busCapacity; i++) {
			float* busStart = &m_state->busMemoryPool[i * samplesPerBus];
			m_state->busPool[i].SetBuffer(std::span<float>(busStart, samplesPerBus));
		}

		m_state->masterBus = &m_state->busPool[MASTER_BUS_INDEX];
		m_state->masterBus->SetName("Master");
		m_state->busPoolMirror[MASTER_BUS_INDEX].isBusy = true;

		m_state->isMixOrderDirty = true;
		m_state->isMixOrderSwapPending = false;
		TryUpdateMixOrder();

		// --- SYSTEMS SETUP ---
		RtSystemConfig rtConfig;
		rtConfig.rtCommands		= m_state->rtCommands.get();
		rtConfig.rtEvents		= m_state->rtEvents.get();
		rtConfig.ioStreamRequests = m_state->ioStreamRequests.get();
		rtConfig.voicePool		= std::span<Voice>(m_state->voicePool.get(), m_state->voiceCapacity);
		rtConfig.streamPool		= std::span<StreamContext>(m_state->streamPool.get(), m_state->streamCapacity);
		rtConfig.busPool		= std::span<Bus>(m_state->busPool.get(), m_state->busCapacity);
		rtConfig.masterBus		= m_state->masterBus;
		m_state->rtSystem = std::make_unique<RtSystem>(rtConfig);

		IoStreamSystemConfig ioStreamingConfig;
		ioStreamingConfig.ioStreamRequests	= m_state->ioStreamRequests.get();
		ioStreamingConfig.streamPool		= std::span<StreamContext>(m_state->streamPool.get(), m_state->streamCapacity);
		ioStreamingConfig.freeStreams		= m_state->freeStreams.get();
		m_state->ioStreamSystem				= std::make_unique<IoStreamSystem>(ioStreamingConfig);

		// --- BACKEND SETUP ---
		m_state->device					= std::make_unique<ma_device>();
		ma_device_config deviceConfig	= ma_device_config_init(ma_device_type_playback);
		deviceConfig.playback.format	= ma_format_f32;
		deviceConfig.playback.channels	= CHANNELS_STEREO; // Stereo playback only
		deviceConfig.sampleRate			= 48000; // Hz
		deviceConfig.dataCallback		= data_callback;
		deviceConfig.periodSizeInFrames = 480;
		deviceConfig.pUserData			= m_state->rtSystem.get();

		if (ma_device_init(NULL, &deviceConfig, m_state->device.get()) != MA_SUCCESS) {
			Logger::Log(LogLevel::Critical, "Device", "Failed to initialize playback device");
			return Result::DeviceFailed;
		}
		// const uint32_t m_periodSize = m_state->device->playback.internalPeriodSizeInFrames;
		// Logger::Log(LogLevel::Debug, "Engine", "Internal Period Size: %d", m_periodSize);


		// --- SYSTEMS START ---
		// I/O Thread Start
		m_state->ioStreamSystem->Start();

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
		m_state->ioStreamSystem->Stop();

		m_state.reset();

		Logger::Log(LogLevel::Info, "Engine", "Deinitialized engine");
		Logger::Deinit();

		return Result::Ok;
	}

	void Engine::Update() {
		if (!m_state->initialized) return;

		// --- Process Event Inbox ---
		RtEvent ev;
		while (m_state->rtEvents->Pop(ev)) {
			switch (ev.type) {
				case RtEvent::Type::MixOrderSwapped: {
					m_state->isUsingMixOrderA = !m_state->isUsingMixOrderA;
					m_state->isMixOrderSwapPending = false;
					break;
				}
				case RtEvent::Type::VoiceFinished: {
					uint32_t index = ev.data.voiceState.index;
					uint32_t generation = ev.data.voiceState.generation;
					if (m_state->voicePoolMirror[index].generation == generation) {
						// Voice is still valid -> Return it to the pool
						m_state->voicePoolMirror[index].Reset();
						m_state->freeVoices->Push(index);
						Logger::Log(LogLevel::Debug, "Engine", "Freed voice %d.", index);
					}
					break;
				}
			}
		}

		TryUpdateMixOrder();

		m_state->rtCommands->Dispatch(); // Send all commands accumulated from this frame to the audio thread
		Logger::ProcessLogs(); // Print all logs accumulated from this frame
	}

	Result Engine::CreateStreamPlayback(PlaybackHandle& handle, const char* filepath) {
		if (!m_state || !m_state->initialized) {
			return Result::NotInitialized;
		}

		uint32_t voiceIndex;
		if (!m_state->freeVoices->Pop(voiceIndex)) {
			Logger::Log(LogLevel::Error, "Engine", "Voice pool exhausted. Failed to create playback instance.");
			return Result::VoicePoolExhausted;
		}

		uint32_t streamIndex;
		if (!m_state->freeStreams->Pop(streamIndex)) {
			m_state->freeVoices->Push(voiceIndex);

			Logger::Log(LogLevel::Error, "Engine", "Stream pool exhausted. Failed to create playback instance.");
			return Result::StreamPoolExhausted;
		}

		VoiceMirror& vMirror = m_state->voicePoolMirror[voiceIndex];
		vMirror.generation++;
		vMirror.state = VoiceState::Inactive;

		// Send I/O request to prepare stream
		m_state->streamPool[streamIndex].state.store(StreamState::Preparing, std::memory_order_release);
		IoStreamRequest req = IoStreamRequest::PrepareStream(streamIndex, filepath);
		if (!m_state->ioStreamRequests->Push(req)) {
			m_state->freeVoices->Push(voiceIndex);

			m_state->streamPool[streamIndex].state.store(StreamState::Free, std::memory_order_release);
			m_state->freeStreams->Push(streamIndex);

			Logger::Log(LogLevel::Error, "Engine", "IO Request queue full. Failed to create playback instance");
			return Result::IoRequestQueueFull;
		}

		// Send command to audio thread (can't fail as it is pushed into a vector)
		RtCommand cmd = RtCommand::PrepareVoiceStreaming(voiceIndex, vMirror.generation, streamIndex);
		m_state->rtCommands->Enqueue(cmd);

		handle = PlaybackHandle::Create(voiceIndex, vMirror.generation);

		return Result::Ok;
	}

	Result Engine::Play(PlaybackHandle handle) {
		if (!m_state || !m_state->initialized) return Result::NotInitialized;

		if (!handle.IsValid()) return Result::InvalidHandle;

		uint32_t voiceIndex = handle.GetIndex();
		uint32_t generation = handle.GetGeneration();
		if (m_state->voicePoolMirror[voiceIndex].generation != generation) {
			return Result::ExpiredHandle;
		}

		RtCommand cmd = RtCommand::PlayVoice(voiceIndex, generation);
		m_state->rtCommands->Enqueue(cmd);

		return Result::Ok;
	}

	void Engine::TryUpdateMixOrder() {
		if (!m_state->isMixOrderDirty || m_state->isMixOrderSwapPending) return;

		uint32_t* backBufferPtr = m_state->isUsingMixOrderA
		? m_state->mixOrderBuffer->listB.get()
		: m_state->mixOrderBuffer->listA.get();

		std::span<uint32_t> backBufferSpan(backBufferPtr, m_state->busCapacity);
		std::span<const BusMirror> busMirror(m_state->busPoolMirror.get(), m_state->busCapacity);

		uint32_t sortedCount = m_state->busGraphCompiler->Compile(busMirror, backBufferSpan);
		if (sortedCount > 0) {
			m_state->rtCommands->Enqueue(RtCommand::SwapMixOrder(backBufferPtr, sortedCount));

			m_state->isMixOrderSwapPending = true;
			m_state->isMixOrderDirty = false;
		}
		else {
			Logger::Log(LogLevel::Critical, "Bus Routing", "Failed to compile mix graph: Cycle detected.");
			m_state->isMixOrderDirty = false;
		}
	}

	void Engine::data_callback(ma_device* pDevice, void* pOutput, const void* pInput, uint32_t frameCount) {
		RtSystem* rtSystem = static_cast<RtSystem*>(pDevice->pUserData);
		rtSystem->OnAudioCallback(static_cast<float*>(pOutput), frameCount, pDevice->playback.channels);
	}
}