#pragma once

#include "messaging/AsyncControlMessaging.h"
#include "core/Constants.h"

#include <thread>
#include <atomic>
#include <semaphore>

namespace dalia {

	class AudioBackend;
	class AsyncMixerSystem;
	class NullOutputDevice;
	class AsyncMessageBus;

	struct AsyncControlSystemConfig {
		AsyncMessageBus* messageRouter = nullptr;

		AudioBackend* deviceManager = nullptr;
		NullOutputDevice* nullOutputDevice = nullptr;
		AsyncMixerSystem* mixerSystem = nullptr;
	};

	class AsyncControlSystem {
	public:
		AsyncControlSystem(const AsyncControlSystemConfig& config);
		~AsyncControlSystem();

		void Start();
		void Stop();

		void NotifyTaskAdded();

	private:
		void ThreadMain();

		std::thread m_thread;
		std::atomic<bool> m_isRunning{false};
		std::counting_semaphore<COUNTING_SEMAPHORE_SIZE> m_taskSemaphore{0};

		AsyncMessageBus* m_messageRouter = nullptr;
	};
}
