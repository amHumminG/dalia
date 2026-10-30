#include "engine/EngineInternalState.h"

#include <algorithm>

namespace dalia {

	EngineInternalState::EngineInternalState(const EngineConfig& config) {
		// Construct message bus
		asyncMessageBus = std::make_unique<AsyncMessageBus>(
			config.advanced.mixerQueueCapacity,
			config.advanced.streamQueueCapacity,
			config.advanced.loadQueueCapacity,
			config.advanced.controlQueueCapacity
		);

		// Construct subsystems
		deviceController = std::make_unique<DeviceController>();
		assetController  = std::make_unique<AssetController>(config.residentSoundCapacity, config.streamSoundCapacity);
		ioController	 = std::make_unique<IoController>(config.streamCapacity);

		MixControllerConfig mixControllerConfig {
			.coordinateSystem = config.coordinateSystem,
			.voiceCapacity    = config.voiceCapacity,
			.listenerCapacity = config.listenerCapacity,
			.busCapacity      = config.busCapacity,
			.biquadCapacity   = config.biquadCapacity
		};
		mixController = std::make_unique<MixController>(mixControllerConfig);
		mixer		  = std::make_unique<Mixer>();

		// Construct threads
		streamThread = std::make_unique<IntervalThread>(
			[this]() { asyncMessageBus->ProcessStreamCommands(); },
			5000, ThreadPriority::High);
		loadThread = std::make_unique<WakeableThread>(
			[this]() { asyncMessageBus->ProcessLoadCommands(); },
			ThreadPriority::Normal
		);
		controlThread = std::make_unique<WakeableThread>(
			[this]() { asyncMessageBus->ProcessControlCommands(); },
			ThreadPriority::Normal
		);
		mixerThread = std::make_unique<WakeableThread>(
			[this]() { asyncMessageBus->ProcessMixerCommands(); },
			ThreadPriority::TimeCritical
		);

		asyncMessageBus->SetLoadWakeupCallback([this]() { loadThread->Wake(); });
		asyncMessageBus->SetControlWakeupCallback([this]() { controlThread->Wake(); });
	}

	EngineInternalState::~EngineInternalState() {
		Shutdown();
	}

	Result EngineInternalState::Initialize(const EngineConfig& config) {
		Result res = ioController->Initialize(asyncMessageBus.get());
		if (res != Result::Ok) return res;

		res = assetController->Initialize(asyncMessageBus.get());
		if (res != Result::Ok) return res;

		res = mixController->Initialize(
			asyncMessageBus.get(),
			assetController.get(),
			ioController.get()
		);
		if (res != Result::Ok) return res;

		mixController->SyncPools();

		res = deviceController->Initialize(
			asyncMessageBus.get(),
			mixer.get(),
			config.sampleRate,
			config.advanced.periodSizeFrames,
			config.startupOutputDeviceId ? config.startupOutputDeviceId : "default"
		);
		if (res != Result::Ok) return res;

		MixerConfig mixerConfig{
			.coordinateSystem = config.coordinateSystem,
			.speakerLayout    = deviceController->GetActiveOutputSpeakerLayout(),
			.periodSizeFrames = deviceController->GetPeriodSizeFrames(),
			.maxSamplesPeriod = deviceController->GetMaxPeriodSizeSamples(),
			.channels         = deviceController->GetActiveOutputChannels(),
			.sampleRate       = deviceController->GetOutputSampleRate(),
			.streamPool       = ioController->GetStreamPoolSpan(),
			.voicePool        = mixController->GetVoicePool().GetMixerView(),
			.listenerPool     = mixController->GetListenerPool().GetMixerView(),
			.busPool          = mixController->GetBusPool().GetMixerView(),
			.biquadPool       = mixController->GetBiquadPool().GetMixerView()
		};
		res = mixer->Initialize(asyncMessageBus.get(), mixerThread.get(), mixerConfig);
		if (res != Result::Ok) return res;

		// Start threads
		controlThread->Start();
		loadThread->Start();
		streamThread->Start();
		mixerThread->Start();

		mixerThread->Wake(); // Pre-warm mixer ring buffer

		res = deviceController->StartActiveOutputDevice(mixer.get());
		if (res != Result::Ok) return res;

		initialized = true;
		return Result::Ok;
	}

	void EngineInternalState::Shutdown() {
		if (!initialized) return;

		// Stop output device
		if (deviceController) deviceController->StopActiveOutputDevice();

		// Stop threads
		if (mixerThread) mixerThread->Stop();
		if (streamThread) streamThread->Stop();
		if (loadThread) loadThread->Stop();
		if (controlThread) controlThread->Stop();

		// Shutdown subsystems
		if (mixer) mixer->Shutdown();
		if (deviceController) deviceController->Shutdown();
		if (mixController) mixController->Shutdown();
		if (assetController) assetController->Shutdown();
		if (ioController) ioController->Shutdown();

		initialized = false;
	}

	bool EngineInternalState::IsInitialized(const EngineInternalState* state) {
		return state != nullptr && state->initialized;
	}
}
