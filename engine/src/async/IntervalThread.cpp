#include "async/IntervalThread.h"

#include "backend/HighResTimer.h"

namespace dalia {

	IntervalThread::IntervalThread(ProcessFunc processFunc, uint32_t intervalMicroseconds, ThreadPriority priority)
		: m_processFunc(std::move(processFunc)), m_intervalMicroseconds(intervalMicroseconds), m_priority(priority) {
	}

	IntervalThread::~IntervalThread() {
		Stop();
	}

	void IntervalThread::Start() {
		if (m_isRunning.load(std::memory_order_relaxed)) return;

		m_isRunning.store(true, std::memory_order_release);
		m_thread = std::thread(&IntervalThread::ThreadMain, this);
	}

	void IntervalThread::Stop() {
		if (!m_isRunning.load(std::memory_order_relaxed)) return;

		m_isRunning.store(false, std::memory_order_release);
		if (m_thread.joinable()) m_thread.join();
	}

	void IntervalThread::ThreadMain() {
		PlatformThread::ScopedThreadRuntime runtime;
		PlatformThread::SetCurrentThreadPriority(m_priority);
		HighResTimer wakeTimer;

		while (m_isRunning.load(std::memory_order_relaxed)) {
			if (m_processFunc) m_processFunc();
			wakeTimer.SleepMicroseconds(m_intervalMicroseconds);
		}
	}
}
