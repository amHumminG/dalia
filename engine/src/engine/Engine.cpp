#include "dalia/Engine.h"

#include "engine/EngineInternalState.h"

#include "dalia/PlaybackControl.h"
#include "dalia/SoundControl.h"
#include "dalia/EffectControl.h"
#include "async/AsyncControlSystem.h"

#include "backend/windows/WindowsDeviceManager.h"
#include "backend/windows/WasapiOutputDevice.h"
#include "backend/NullOutputDevice.h"

#include "core/Logger.h"
#include "core/SPSCRingBuffer.h"
#include "core/Constants.h"
#include "core/Types.h"
#include "core/Math.h"

#include "messaging/AsyncMixerMessaging.h"
#include "messaging/AsyncStreamMessaging.h"
#include "messaging/AsyncLoadMessaging.h"
#include "messaging/AsyncControlMessaging.h"

#include "core/StateSyncPool.h"
#include "core/AsyncPool.h"
#include "mixer/Voice.h"
#include "mixer/StreamContext.h"
#include "mixer/Bus.h"
#include "mixer/effects/Biquad.h"
#include "mixer/MixGraphCompiler.h"
#include "mixer/Listener.h"
#include "mixer/AsyncMixerSystem.h"
#include "mixer/Speakers.h"
#include "mixer/effects/EffectTraits.h"

#include "async/AsyncStreamSystem.h"
#include "async/AsyncLoadSystem.h"

#include "resources/AssetRegistry.h"
#include "resources/ResidentSound.h"
#include "resources/StreamSound.h"

#include "engine/EngineHelpers.h"

#include "common/StringID.h"

#include <future>

#if defined(min)
#undef min
#endif

#if defined(max)
#undef max
#endif

namespace dalia {

	// --- INTERNAL HELPERS ---

	/// @brief Tears down the engine internal state.
	/// @param state The engine internal state.
	static void TeardownInternal(EngineInternalState* state) {
		if (!state) return;

		state->deviceCoordinator.Shutdown();

		if (state->asyncMixerSystem)			state->asyncMixerSystem->Stop();
		if (state->asyncLoadSystem)		state->asyncLoadSystem->Stop();
		if (state->asyncStreamSystem)	state->asyncStreamSystem->Stop();
		if (state->asyncControlSystem)	state->asyncControlSystem->Stop();

		delete state;
		state = nullptr;

		Logger::Deinit();
	};

	// ------------------------

	Engine::Engine() = default;
	Engine::~Engine() { TeardownInternal(m_state); };

	Result Engine::Initialize(const EngineConfig& config) {
		if (m_state != nullptr) {
			DALIA_LOG_WARN(LOG_CTX_API, "Attempting to initialize engine that is already initialized.");
			return Result::AlreadyInitialized;
		}

		Logger::Init(config.logLevel, 256);
		if (config.logCallback != nullptr) Logger::SetSink(config.logCallback);

		// --- INTERNAL STATE SETUP ---
		m_state = new EngineInternalState(config);

		// --- Master Bus Setup ---
		uint32_t masterIndex;
		m_state->buses.Allocate(masterIndex);
		m_state->buses.GetMirror(MASTER_BUS_INDEX).refCount = 1;
		m_state->buses.Get(MASTER_BUS_INDEX).isActive = true;
		constexpr BusID masterId("Master");
		m_state->busMap[masterId] = MASTER_BUS_INDEX;

		// --- Listener 0 Setup ---
		m_state->listeners.Get(0).params.isActive = true;
		m_state->listeners.GetMirror(0).params.isActive = true;

		m_state->deviceCoordinator.Initialize(
			&m_state->messageRouter,
			&m_state->asyncMixerSystem,
			config.sampleRate,
			config.advanced.periodSizeFrames,
			config.startupOutputDeviceId
		);

		// -- Periodicity Dependent Buffer Allocations ---
		uint32_t maxPeriodSizeSamples = m_state->deviceCoordinator.GetMaxPeriodSizeSamples();
		uint32_t busBufferPoolSize = m_state->buses.Capacity() * m_state->deviceCoordinator.GetMaxPeriodSizeSamples();
		m_state->busBufferPool = std::make_unique<float[]>(busBufferPoolSize);
		m_state->mixScratchBuffer = std::make_unique<float[]>(maxPeriodSizeSamples);

		// --- SYSTEMS SETUP ---
		MixerSystemConfig mixerConfig;
		mixerConfig.coordinateSystem		= m_state->coordinateSystem;
		mixerConfig.speakerLayout			= m_state->deviceCoordinator.GetActiveOutputSpeakerLayout();
		mixerConfig.maxSamplesPerPeriod		= maxPeriodSizeSamples;
		mixerConfig.outChannels				= m_state->deviceCoordinator.GetActiveOutputChannels();
		mixerConfig.outSampleRate			= m_state->deviceCoordinator.GetOutputSampleRate();
		mixerConfig.asyncDirector			= &m_state->messageRouter;
		mixerConfig.streamPool				= m_state->streams.GetSpan();
		mixerConfig.listenerPool			= m_state->listeners.GetSpan();
		mixerConfig.listenerParamBridges	= m_state->listeners.GetParamBridgeSpan();
		mixerConfig.voicePool				= m_state->voices.GetSpan();
		mixerConfig.voiceParamBridges		= m_state->voices.GetParamBridgeSpan();
		mixerConfig.busPool					= m_state->buses.GetSpan();
		mixerConfig.busParamBridges			= m_state->buses.GetParamBridgeSpan();
		mixerConfig.busBufferPool			= std::span(m_state->busBufferPool.get(), busBufferPoolSize);
		mixerConfig.biquadPool				= m_state->biquads.GetSpan();
		mixerConfig.biquadParamBridges		= m_state->biquads.GetParamBridgeSpan();
		mixerConfig.mixGraphCompiler		= m_state->mixGraphCompiler.get();
		mixerConfig.mixOrder				= std::span(m_state->mixOrder.get(), m_state->buses.Capacity());
		mixerConfig.dspScratchBuffer		= std::span(m_state->mixScratchBuffer.get(), maxPeriodSizeSamples);
		m_state->asyncMixerSystem = std::make_unique<AsyncMixerSystem>(mixerConfig);

		AsyncStreamSystemConfig asyncStreamConfig;
		asyncStreamConfig.wakeupPeriodMicroseconds  = STREAM_SYSTEM_WAKEUP_PERIOD_MICROSECONDS;
		asyncStreamConfig.outSampleRate				= m_state->deviceCoordinator.GetOutputSampleRate();
		asyncStreamConfig.asyncDirector				= &m_state->messageRouter;
		asyncStreamConfig.streamPool				= m_state->streams.GetSpan();
		asyncStreamConfig.freeStreams				= m_state->streams.GetFreeList();
		m_state->asyncStreamSystem	= std::make_unique<AsyncStreamSystem>(asyncStreamConfig);

		AsyncLoadSystemConfig asyncLoadConfig;
		asyncLoadConfig.outSampleRate	= m_state->deviceCoordinator.GetOutputSampleRate();
		asyncLoadConfig.messageRouter	= &m_state->messageRouter;
		asyncLoadConfig.assetRegistry	= m_state->assetRegistry.get();
		m_state->asyncLoadSystem = std::make_unique<AsyncLoadSystem>(asyncLoadConfig);

		AsyncControlSystemConfig asyncControlSystemConfig;
		asyncControlSystemConfig.messageRouter	= &m_state->messageRouter;
		asyncControlSystemConfig.mixerSystem	= m_state->asyncMixerSystem.get();
		m_state->asyncControlSystem = std::make_unique<AsyncControlSystem>(asyncControlSystemConfig);

		// --- SYSTEMS START ---
		m_state->asyncMixerSystem->Start();
		m_state->asyncStreamSystem->Start();
		m_state->asyncLoadSystem->Start();
		m_state->asyncControlSystem->Start();

		Result res = m_state->deviceCoordinator.StartActiveOutputDevice(m_state->asyncMixerSystem.get());
		if (res != Result::Ok) {
			DALIA_LOG_CRIT(LOG_CTX_API, "Failed to initialize engine. Failed to start audio thread.");
			TeardownInternal(m_state);
			return res;
		}

		m_state->initialized = true;
		DALIA_LOG_INFO(LOG_CTX_API, "Initialized engine.");
		return Result::Ok;
	}

	Result Engine::Shutdown() {
		if (!EngineInternalState::IsInitialized(m_state)) {
			DALIA_LOG_WARN(LOG_CTX_API, "Attempting to shutdown uninitialized engine.");
			return Result::NotInitialized;
		}

		DALIA_LOG_INFO(LOG_CTX_API, "Engine shutdown.");
		TeardownInternal(m_state);

		return Result::Ok;
	}

	void Engine::Update() {
		if (!EngineInternalState::IsInitialized(m_state)) return;

		// --- Process Event Inbox ---
		m_state->messageRouter.ProcessMainEvents();

		// --- Sync Pools ---
		m_state->voices.Sync();
		m_state->listeners.Sync();
		m_state->buses.Sync();
		m_state->biquads.Sync();

		m_state->messageRouter.DispatchStagedCommands(); // Send all commands accumulated from this frame to the audio thread
		Logger::ProcessLogs(); // Print all logs accumulated from this frame
	}
}
