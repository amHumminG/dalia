#include "async/WakeableThread.h"

namespace dalia {

	WakeableThread::WakeableThread(std::function<void()> processFunc, ThreadPriority priority)
		: m_processFunc(std::move(processFunc)), m_priority(priority) {
	}

	WakeableThread::~WakeableThread() {
		Stop();
	}

	void WakeableThread::Start() {
		if (m_isRunning.load(std::memory_order_relaxed)) return;

		m_isRunning.store(true, std::memory_order_release);
		m_thread = std::thread(&WakeableThread::ThreadMain, this);
	}

	void WakeableThread::Stop() {
		if (!m_isRunning.load(std::memory_order_relaxed)) return;

		m_isRunning.store(false, std::memory_order_release);
		m_wakeSemaphore.release();
		if (m_thread.joinable()) m_thread.join();
	}

	void WakeableThread::Wake() {
		m_wakeSemaphore.release();
	}

	void WakeableThread::ThreadMain() {
		PlatformThread::ScopedThreadRuntime runtime;
		PlatformThread::SetCurrentThreadPriority(m_priority);

		while (m_isRunning.load(std::memory_order_relaxed)) {
			m_wakeSemaphore.acquire();
			if (!m_isRunning.load(std::memory_order_relaxed)) break;

			if (m_processFunc) m_processFunc();
		}
	}
}
