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

#include "messaging/MixerMessaging.h"
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

	static void ProcessOutputDeviceSwapping(EngineInternalState* state) {
		// --- Phase 1 (Poll event queue) ---
		AsyncControlEvent ev;
		while (state->asyncControlEvents->Pop(ev)) {
			if (ev.type == AsyncControlEvent::Type::OutputDeviceSwapped) {
				state->outputDevice.reset(ev.data.swapOutputDevice.newDevice); // Take ownership of new device (already started)

				if (ev.data.swapOutputDevice.fellBackToDefault) {
					state->targetOutputDeviceId = "default";
					DALIA_LOG_WARN(LOG_CTX_API, "Requested audio output device was unavailable. Fell back to OS default.");
				}

				state->activeOutputDeviceIsDefault = (state->targetOutputDeviceId == "default");

				if (state->outputDevice) {
					state->activeOutputDevice = state->outputDevice.get();
					state->outChannels = state->activeOutputDevice->GetChannels();
					state->speakerLayout = state->activeOutputDevice->GetSpeakerLayout();

					std::string layout;
					switch (state->speakerLayout) {
						case SpeakerLayout::Mono: layout = "Mono"; break;
						case SpeakerLayout::Stereo: layout = "Stereo"; break;
						case SpeakerLayout::Surround51: layout = "5.1"; break;
						case SpeakerLayout::Surround71: layout = "7.1"; break;
					}
					DALIA_LOG_INFO(LOG_CTX_API, "Successfully swapped audio device to \"%s\". %s layout with %u channel(s).",
						state->outputDevice->GetName().c_str(), layout.c_str(), state->outChannels);
				}
				else {
					// Both the requested device and the OS default device failed
					state->activeOutputDevice = state->nullOutputDevice.get(); // Fall back to null device
					state->outChannels = state->activeOutputDevice->GetChannels();
					state->speakerLayout = state->activeOutputDevice->GetSpeakerLayout();
					state->targetOutputDeviceId = "default";

					DALIA_LOG_ERR(LOG_CTX_API, "No audio output devices available. Audio will render to void for now.");
				}

				state->isSwappingOutputDevice = false;
			}
		}

		// --- Phase 2 (Wait For Ongoing Swap) ---
		if (state->isSwappingOutputDevice) return;

		// --- Phase 3 (Initiate Swap If Needed) ---
		std::string newOsDefaultOutputId;
		bool osDefaultOutputChanged = state->deviceManager->PopDeviceChangeNotification(newOsDefaultOutputId);
		bool shouldReactToOsDefaultChanged = osDefaultOutputChanged && (state->targetOutputDeviceId == "default");
		bool manualChangePending = state->pendingOutputDeviceSwap;
		bool deviceFailed = state->activeOutputDevice && state->activeOutputDevice->HasFailed();

		// Async shield (for double swaps)
		if (shouldReactToOsDefaultChanged && state->activeOutputDevice && !deviceFailed) {
			// Does the active device's id match the new os default id?
			if (state->activeOutputDevice->GetIdentifier() == newOsDefaultOutputId) {
				// We are already using the new default device -> ignore os notification
				shouldReactToOsDefaultChanged = false;
			}
		}

		if (shouldReactToOsDefaultChanged || manualChangePending || deviceFailed) {
			state->pendingOutputDeviceSwap = false;
			state->isSwappingOutputDevice = true;

			if (deviceFailed) DALIA_LOG_INFO(LOG_CTX_API, "Active audio device reported failure. Initiating swap sequence.");
			else if (manualChangePending) DALIA_LOG_INFO(LOG_CTX_API, "Audio device swap requested. Initiating swap sequence.");
			else DALIA_LOG_INFO(LOG_CTX_API, "OS default audio device changed. Initiating swap sequence.");

			// Release ownership of active device
			OutputDevice* deviceToKill = state->outputDevice.release();
			state->activeOutputDevice = nullptr; // Temporarily without output

			// Push swap request to async control system
			AsyncControlCommand req = AsyncControlCommand::SwapOutputDevice(
				deviceToKill,
				state->targetOutputDeviceId.c_str(),
				state->outSampleRate
			);

			bool wasEmpty = false;
			if (state->asyncControlRequests->Push(req, wasEmpty)) {
				if (wasEmpty) state->asyncControlSystem->NotifyTaskAdded(); // Wake up control thread
			}
			else {
				DALIA_LOG_ERR(LOG_CTX_API, "Failed to push control request. Queue is full.");
			}
		}
	}



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

		// --- Device Manager Setup ---
#ifdef _WIN32
		m_state->deviceManager = std::make_unique<WindowsDeviceManager>();
#else
		DALIA_LOG_CRIT(LOG_CTX_API, "Failed to initialize engine. Unsupported OS.");
		TeardownInternal();
		return Result::SystemError;
#endif

		Result res = m_state->deviceManager->Initialize();
		if (res != Result::Ok) {
			DALIA_LOG_CRIT(LOG_CTX_API, "Failed to initialize engine. Device manager initialization failed.");
			TeardownInternal(m_state);
			return res;
		}

		// --- Device & Null-Device Setup ---
		m_state->outSampleRate = config.sampleRate;

		m_state->outputDevice = m_state->deviceManager->CreateDevice("default", m_state->outSampleRate);

		m_state->nullOutputDevice = std::make_unique<NullOutputDevice>(
			m_state->outSampleRate,
			(m_state->outSampleRate * 10) / 1000
		);

		if (m_state->outputDevice) {
			m_state->activeOutputDevice = m_state->outputDevice.get();

			m_state->nullOutputDevice->SetOutputFormat(
				m_state->activeOutputDevice->GetChannels(),
				m_state->activeOutputDevice->GetSpeakerLayout()
			);
		}
		else {
			m_state->activeOutputDevice = m_state->nullOutputDevice.get();
		}

		m_state->outChannels = m_state->activeOutputDevice->GetChannels();
		m_state->speakerLayout = m_state->activeOutputDevice->GetSpeakerLayout();

		if (m_state->outputDevice) {
			std::string layout;
			switch (m_state->speakerLayout) {
				case SpeakerLayout::Mono: layout = "Mono"; break;
				case SpeakerLayout::Stereo: layout = "Stereo"; break;
				case SpeakerLayout::Surround51: layout = "5.1"; break;
				case SpeakerLayout::Surround71: layout = "7.1"; break;
			}
			DALIA_LOG_INFO(LOG_CTX_API, "Initialized audio device \"%s\". %s layout with %u channel(s).",
				m_state->outputDevice->GetName().c_str(), layout.c_str(), m_state->outChannels);
		}
		else {
			DALIA_LOG_WARN(LOG_CTX_API, "No output audio device found. Audio will render to void for now.");
		}

		// -- Periodicity Dependent Buffer Allocations ---
		m_state->maxSamplesPerPeriod = MIXER_PROCESSING_BLOCK_FRAMES * CHANNELS_MAX;
		uint32_t busBufferPoolSize = m_state->buses.Capacity() * m_state->maxSamplesPerPeriod;
		m_state->busBufferPool = std::make_unique<float[]>(busBufferPoolSize);
		m_state->mixScratchBuffer = std::make_unique<float[]>(m_state->maxSamplesPerPeriod);

		// --- SYSTEMS SETUP ---
		MixerSystemConfig mixerConfig;
		mixerConfig.coordinateSystem		= m_state->coordinateSystem;
		mixerConfig.speakerLayout			= m_state->speakerLayout;
		mixerConfig.maxSamplesPerPeriod	= m_state->maxSamplesPerPeriod;
		mixerConfig.outChannels			= m_state->outChannels;
		mixerConfig.outSampleRate			= m_state->outSampleRate;
		mixerConfig.mixerCommands			= m_state->mixerCommands.get();
		mixerConfig.mixerEvents			= m_state->mixerEvents.get();
		mixerConfig.asyncStreamRequests	= m_state->asyncStreamRequests.get();
		mixerConfig.streamPool				= m_state->streams.GetSpan();
		mixerConfig.listenerPool			= m_state->listeners.GetSpan();
		mixerConfig.listenerParamBridges	= m_state->listeners.GetParamBridgeSpan();
		mixerConfig.voicePool				= m_state->voices.GetSpan();
		mixerConfig.voiceParamBridges		= m_state->voices.GetParamBridgeSpan();
		mixerConfig.busPool				= m_state->buses.GetSpan();
		mixerConfig.busParamBridges		= m_state->buses.GetParamBridgeSpan();
		mixerConfig.busBufferPool			= std::span(m_state->busBufferPool.get(), busBufferPoolSize);
		mixerConfig.biquadPool				= m_state->biquads.GetSpan();
		mixerConfig.biquadParamBridges		= m_state->biquads.GetParamBridgeSpan();
		mixerConfig.mixGraphCompiler		= m_state->mixGraphCompiler.get();
		mixerConfig.mixOrder				= std::span(m_state->mixOrder.get(), m_state->buses.Capacity());
		mixerConfig.dspScratchBuffer		= std::span(m_state->mixScratchBuffer.get(), m_state->maxSamplesPerPeriod);
		m_state->asyncMixerSystem = std::make_unique<AsyncMixerSystem>(mixerConfig);

		AsyncStreamSystemConfig asyncStreamConfig;
		asyncStreamConfig.wakeupPeriodMicroseconds = STREAM_SYSTEM_WAKEUP_PERIOD_MICROSECONDS;
		asyncStreamConfig.outSampleRate				= m_state->outSampleRate;
		asyncStreamConfig.ioStreamRequests			= m_state->asyncStreamRequests.get();
		asyncStreamConfig.streamPool				= m_state->streams.GetSpan();
		asyncStreamConfig.freeStreams				= m_state->streams.GetFreeList();
		m_state->asyncStreamSystem	= std::make_unique<AsyncStreamSystem>(asyncStreamConfig);

		AsyncLoadSystemConfig asyncLoadConfig;
		asyncLoadConfig.outSampleRate	= m_state->outSampleRate;
		asyncLoadConfig.ioLoadRequests	= m_state->asyncLoadRequests.get();
		asyncLoadConfig.ioLoadEvents	= m_state->asyncLoadEvents.get();
		asyncLoadConfig.assetRegistry	= m_state->assetRegistry.get();
		m_state->asyncLoadSystem = std::make_unique<AsyncLoadSystem>(asyncLoadConfig);

		AsyncControlSystemConfig asyncControlSystemConfig;
		asyncControlSystemConfig.requestQueue = m_state->asyncControlRequests.get();
		asyncControlSystemConfig.eventQueue = m_state->asyncControlEvents.get();
		asyncControlSystemConfig.deviceManager = m_state->deviceManager.get();
		asyncControlSystemConfig.nullOutputDevice = m_state->nullOutputDevice.get();
		asyncControlSystemConfig.mixerSystem = m_state->asyncMixerSystem.get();
		m_state->asyncControlSystem = std::make_unique<AsyncControlSystem>(asyncControlSystemConfig);

		// --- SYSTEMS START ---
		m_state->asyncMixerSystem->Start();
		m_state->asyncStreamSystem->Start();
		m_state->asyncLoadSystem->Start();
		m_state->asyncControlSystem->Start();

		res = m_state->activeOutputDevice->Start(m_state->asyncMixerSystem.get());
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

		ProcessOutputDeviceSwapping(m_state);

		// --- Process Event Inbox ---
		AsyncMixerEvent RtEv;
		while (m_state->mixerEvents->Pop(RtEv)) ProcessMixerEvent(m_state, RtEv);

		AsyncLoadEvent loadEv;
		while (m_state->asyncLoadEvents->Pop(loadEv)) ProcessAsyncLoadEvent(m_state, loadEv);

		// --- Sync Pools ---
		m_state->voices.Sync();
		m_state->listeners.Sync();
		m_state->buses.Sync();
		m_state->biquads.Sync();

		m_state->mixerCommands->Dispatch(); // Send all commands accumulated from this frame to the audio thread
		Logger::ProcessLogs(); // Print all logs accumulated from this frame
	}
}
