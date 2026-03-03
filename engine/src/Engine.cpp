#include "dalia/audio/Engine.h"
#include "dalia/audio/PlaybackControl.h"
#include "core/Logger.h"
#include "core/FixedStack.h"
#include "core/SPSCRingBuffer.h"
#include "messaging/RtCommandQueue.h"
#include "messaging/RtEventQueue.h"
#include "messaging/IoRequestQueue.h"
#include "mixer/Voice.h"
#include "mixer/StreamingContext.h"
#include "mixer/Bus.h"
#include "mixer/BusGraphCompiler.h"

#include "systems/RealTimeSystem.h"
#include "systems/IoSystem.h"

#include <span>

#include "miniaudio.h"

namespace dalia {

	static constexpr uint8_t CHANNELS_STEREO = 2;

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
		std::unique_ptr<MixOrderBuffer>		mixOrderBuffer;
		std::unique_ptr<BusGraphCompiler>	busGraphCompiler;

		bool isUsingMixOrderA		= true;
		bool isMixOrderDirty		= false;
		bool isMixOrderSwapPending	= false;

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

		m_state.reset();

		Logger::Log(LogLevel::Info, "Engine", "Deinitialized engine");
		Logger::Deinit();

		return Result::Ok;
	}

	void Engine::Update() {
		if (!m_state->initialized) return;

		// --- Process Event Inbox ---
		RtEvent ev;
		while (m_state->rtEventQueue->Pop(ev)) {
			switch (ev.type) {
				case RtEvent::Type::VoiceFinished: {
					// TODO: Implement
					// Trigger callback
					// Return voice to pool
					break;
				}
				case RtEvent::Type::PlaybackError: {
					// TODO: Implement
					break;
				}
				case RtEvent::Type::GraphSwapped: {
					m_state->isUsingMixOrderA = !m_state->isUsingMixOrderA;
					m_state->isMixOrderSwapPending = false;
					break;
				}
			}
		}

		// --- Process Command Outbox ---
		// Mix Order
		if (m_state->isMixOrderDirty && !m_state->isMixOrderSwapPending) {
			uint32_t* backBufferPtr = m_state->isUsingMixOrderA
			? m_state->mixOrderBuffer->listB.get()
			: m_state->mixOrderBuffer->listA.get();

			std::span<uint32_t> backBufferSpan(backBufferPtr, m_state->busCapacity);
			std::span<const BusMirror> busMirror(m_state->busPoolMirror.get(), m_state->busCapacity);

			uint32_t sortedCount = m_state->busGraphCompiler->Compile(busMirror, backBufferSpan);
			if (sortedCount > 0) {
				m_state->rtCommandQueue->Enqueue(RtCommand::SwapMixOrder(backBufferPtr, sortedCount));
				m_state->isMixOrderSwapPending = true;
				m_state->isMixOrderDirty = false;
			}
			else {
				Logger::Log(LogLevel::Critical, "Bus Routing", "Failed to compile mix graph: Cycle detected.");
				m_state->isMixOrderDirty = false;
			}
		}

		m_state->rtCommandQueue->Dispatch(); // Send all commands accumulated from this frame to the audio thread
		Logger::ProcessLogs(); // Print all logs accumulated from this frame
	}

	void Engine::data_callback(ma_device* pDevice, void* pOutput, const void* pInput, uint32_t frameCount) {
		RealTimeSystem* rtSystem = static_cast<RealTimeSystem*>(pDevice->pUserData);
		rtSystem->OnAudioCallback(static_cast<float*>(pOutput), frameCount, pDevice->playback.channels);
	}
}