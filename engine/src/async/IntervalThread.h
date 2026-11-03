#pragma once

#include "platform/PlatformThread.h"

#include <thread>
#include <atomic>
#include <functional>
#include <cstdint>
#include <string>

namespace dalia {

	class IntervalThread {
	public:
		using ProcessFunc = std::function<void()>;

		IntervalThread(const std::string& name, ProcessFunc processFunc, uint32_t intervalMicroseconds,
			ThreadPriority priority = ThreadPriority::Normal);
		~IntervalThread();

		/// @brief Starts the thread.
		void Start();

		/// @brief Stops the thread.
		void Stop();

	private:
		/// @brief The function that the thread invokes on start.
		void ThreadMain();

		std::thread m_thread;
		std::atomic<bool> m_isRunning{false};
		std::binary_semaphore m_wakeSemaphore{0};

		std::string m_name;
		ProcessFunc m_processFunc = nullptr;
		uint32_t m_intervalMicroseconds = 0;
		ThreadPriority m_priority = ThreadPriority::Normal;
	};
}