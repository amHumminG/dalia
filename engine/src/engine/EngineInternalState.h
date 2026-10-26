#pragma once

#include "dalia/Engine.h"

// --- NEW ---
#include "async/messaging/AsyncMessageBus.h"

#include "backend/DeviceController.h"
#include "asset/AssetController.h"
#include "io/IoController.h"
#include "mixer/MixController.h"
// #include "mixer/Mixer.h"

#include "async/WakeableThread.h"
#include "async/IntervalThread.h"
// ------

#include <memory>

namespace dalia {

	class AudioBackend;
	class OutputDevice;
	class NullOutputDevice;
	class MixerController;
	class AsyncStreamSystem;
	class AsyncLoadSystem;
	class AsyncControlSystem;
	class AssetRegistry;
	class MixGraphCompiler;

	struct StreamContext;

	/// @brief The internal implementation state of the DALIA engine.
	struct EngineInternalState {
		bool initialized = false;

		std::unique_ptr<AsyncMessageBus>	asyncMessageBus;

		std::unique_ptr<DeviceController>	deviceController;
		std::unique_ptr<AssetController>	assetController;
		std::unique_ptr<IoController>		ioController;
		std::unique_ptr<MixController>		mixController;
		// std::unique_ptr<Mixer>			mixer;

		// Threads
		std::unique_ptr<IntervalThread> streamThread;
		std::unique_ptr<WakeableThread> loadThread;
		std::unique_ptr<WakeableThread> controlThread;

		// DSP Utility
		std::unique_ptr<MixGraphCompiler> mixGraphCompiler;	// TODO: Move to mixer
		std::unique_ptr<uint32_t[]> mixOrder;				// TODO: Move to mixer
		std::unique_ptr<float[]> mixScratchBuffer;			// TODO: Move to mixer

		// Systems
		std::unique_ptr<MixerController>	asyncMixerSystem;	// TODO: Rework

		EngineInternalState(const EngineConfig& config);

		/// @brief Engine helper to check if internal state is initialized.
		/// @param state The engine internal state.
		/// @return True if initialized, otherwise false.
		static bool IsInitialized(const EngineInternalState* state);
	};
}
