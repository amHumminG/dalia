#pragma once

#include <thread>
#include <atomic>
#include <semaphore>

namespace dalia {

	template <typename T> class SPSCRingBuffer;
	struct AsyncControlRequest;
	struct AsyncControlEvent;
	class DeviceManager;

	struct AsyncControlSystemConfig {
		SPSCRingBuffer<AsyncControlRequest>* requestQueue = nullptr;
		SPSCRingBuffer<AsyncControlEvent>* eventQueue = nullptr;
		DeviceManager* deviceManager = nullptr;
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

		SPSCRingBuffer<AsyncControlRequest>* m_requestQueue = nullptr;
		SPSCRingBuffer<AsyncControlEvent>* m_eventQueue = nullptr;
		DeviceManager* m_deviceManager = nullptr;
	};
}
