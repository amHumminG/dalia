#pragma once

#include "dalia/Engine.hpp"

// --- NEW ---
#include "async/AsyncMessageBus.hpp"

#include "platform/DeviceController.hpp"
#include "asset/AssetController.hpp"
#include "io/IoController.hpp"
#include "mixer/MixController.hpp"
#include "mixer/Mixer.hpp"

#include "async/WakeableThread.hpp"
#include "async/IntervalThread.hpp"
// ------

#include <memory>

namespace dalia {

	/// @brief The internal implementation state of the DALIA engine.
	struct EngineInternalState {
		bool initialized = false;

		// Cross-Thread Messaging
		std::unique_ptr<AsyncMessageBus> asyncMessageBus;

		// Controllers
		std::unique_ptr<DeviceController> deviceController;
		std::unique_ptr<AssetController> assetController;
		std::unique_ptr<IoController> ioController;
		std::unique_ptr<MixController> mixController;
		std::unique_ptr<Mixer> mixer;

		// Threads
		std::unique_ptr<IntervalThread> streamThread;
		std::unique_ptr<WakeableThread> loadThread;
		std::unique_ptr<WakeableThread> controlThread;
		std::unique_ptr<WakeableThread> mixerThread;

		EngineInternalState(const EngineConfig& config);
		~EngineInternalState();

		Result Initialize(const EngineConfig& config);
		void Shutdown();

		/// @brief Engine helper to check if internal state is initialized.
		/// @param state The engine internal state.
		/// @return True if initialized, otherwise false.
		static bool IsInitialized(const EngineInternalState* state);
	};
}
