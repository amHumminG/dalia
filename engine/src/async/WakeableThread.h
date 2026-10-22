#pragma once

#include <thread>
#include <atomic>
#include <functional>

#include "backend/PlatformThread.h"

namespace dalia {

	class WakeableThread {
	public:
		using ProcessFunc = std::function<void()>;

		WakeableThread(ProcessFunc processFunc, ThreadPriority priority = ThreadPriority::Normal);
		~WakeableThread();

		/// @brief Starts the thread.
		void Start();

		/// @brief Stops the thread.
		void Stop();

		/// @brief Wakes up the thread to execute its assigned processing function.
		void Wake();

	private:
		/// @brief The function that the thread invokes on start.
		void ThreadMain();

		std::thread m_thread;
		std::atomic<bool> m_isRunning{false};
		std::binary_semaphore m_wakeSemaphore{0};

		ProcessFunc m_processFunc = nullptr;
		ThreadPriority m_priority = ThreadPriority::Normal;
	};
}
