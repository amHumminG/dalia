#pragma once

#include "messaging/AsyncControlMessaging.h"

#include <thread>
#include <atomic>
#include <semaphore>

namespace dalia {

	class DeviceManager;
	class RtSystem;

	struct AsyncControlSystemConfig {
		AsyncControlRequestQueue* requestQueue = nullptr;
		AsyncControlEventQueue* eventQueue = nullptr;

		DeviceManager* deviceManager = nullptr;
		OutputDevice* nullOutputDevice = nullptr;
		RtSystem* rtSystem = nullptr;
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
		void ProcessRequest(const AsyncControlRequest& req);

		std::thread m_thread;
		std::atomic<bool> m_isRunning{false};
		std::counting_semaphore<1024> m_taskSemaphore{0};

		AsyncControlRequestQueue* m_requestQueue = nullptr;
		AsyncControlEventQueue* m_eventQueue = nullptr;

		DeviceManager* m_deviceManager = nullptr;
		OutputDevice* m_nullOutputDevice = nullptr;
		RtSystem* m_rtSystem = nullptr;
	};
}
