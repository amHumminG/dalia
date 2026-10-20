#pragma once

#include "messaging/AsyncControlMessaging.h"
#include "core/Constants.h"

#include <thread>
#include <atomic>
#include <semaphore>

namespace dalia {

	class DeviceManager;
	class AsyncMixerSystem;
	class NullOutputDevice;

	struct AsyncControlSystemConfig {
		AsyncControlCommandQueue* requestQueue = nullptr;
		AsyncControlEventQueue* eventQueue = nullptr;

		DeviceManager* deviceManager = nullptr;
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
		void ProcessRequest(const AsyncControlCommand& req);

		std::thread m_thread;
		std::atomic<bool> m_isRunning{false};
		std::counting_semaphore<COUNTING_SEMAPHORE_SIZE> m_taskSemaphore{0};

		AsyncControlCommandQueue* m_requestQueue = nullptr;
		AsyncControlEventQueue* m_eventQueue = nullptr;

		DeviceManager* m_deviceManager = nullptr;
		NullOutputDevice* m_nullOutputDevice = nullptr;
		AsyncMixerSystem* m_mixerSystem = nullptr;
	};
}
