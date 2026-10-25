#include "engine/EngineInternalState.h"

#include "mixer/AsyncMixerSystem.h"
#include "mixer/MixGraphCompiler.h"
#include "mixer/StreamContext.h"

#include <algorithm>

namespace dalia {

	EngineInternalState::EngineInternalState(const EngineConfig& config)
		: asyncMessageBus(config.advanced.mixerQueueCapacity, config.advanced.streamQueueCapacity,
			config.advanced.loadQueueCapacity, config.advanced.controlQueueCapacity),


		coordinateSystem(config.coordinateSystem), voices(config.voiceCapacity),
		listeners(std::clamp(config.listenerCapacity, LISTENERS_MIN, LISTENERS_MAX)),
		buses(config.busCapacity), biquads(config.biquadCapacity) {

		// Allocate subsystems
		deviceController = std::make_unique<DeviceController>();
		assetController = std::make_unique<AssetController>();
		// mixer = std::make_unique<Mixer>();

		// Allocate threads
		streamThread = std::make_unique<IntervalThread>(
			[this]() { asyncMessageBus.ProcessStreamCommands(); },
			5000, ThreadPriority::High);
		loadThread = std::make_unique<WakeableThread>(
			[this]() { asyncMessageBus.ProcessLoadCommands(); },
			ThreadPriority::Normal
		);
		controlThread = std::make_unique<WakeableThread>(
			[this]() { asyncMessageBus.ProcessControlCommands(); },
			ThreadPriority::Normal
		);
		asyncMessageBus.SetLoadWakeupCallback([this]() { loadThread->Wake(); });
		asyncMessageBus.SetControlWakeupCallback([this]() { controlThread->Wake(); });

		// Mixing
		mixGraphCompiler	= std::make_unique<MixGraphCompiler>(config.busCapacity);
		mixOrder			= std::make_unique<uint32_t[]>(config.busCapacity);
	}

	bool EngineInternalState::IsInitialized(const EngineInternalState* state) {
		return state != nullptr && state->initialized;
	}
}
