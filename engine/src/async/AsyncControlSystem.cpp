#include "async/AsyncControlSystem.h"

#include "messaging/MessageRouter.h"
#include "backend/PlatformThread.h"

namespace dalia {

	AsyncControlSystem::AsyncControlSystem(const AsyncControlSystemConfig& config)
		: m_messageRouter(config.messageRouter) {
		m_messageRouter->SetControlWakeupCallback([this](){ NotifyTaskAdded(); });
	}

	AsyncControlSystem::~AsyncControlSystem() {
		Stop();
	}

	void AsyncControlSystem::Start() {
		if (m_isRunning.load(std::memory_order_relaxed)) return;

		m_isRunning.store(true, std::memory_order_release);
		m_thread = std::thread(&AsyncControlSystem::ThreadMain, this);
	}

	void AsyncControlSystem::Stop() {
		if (!m_isRunning.load(std::memory_order_relaxed)) return;

		m_isRunning.store(false, std::memory_order_release);
		m_taskSemaphore.release(); // Wake thread so it can perform safe exit
		if (m_thread.joinable()) m_thread.join();
	}

	void AsyncControlSystem::NotifyTaskAdded() {
		m_taskSemaphore.release();
	}

	void AsyncControlSystem::ThreadMain() {
		PlatformThread::ScopedThreadRuntime runtime;

		while (m_isRunning.load(std::memory_order_relaxed)) {
			m_taskSemaphore.acquire(); // Sleep (wake on notification)
			if (!m_isRunning.load(std::memory_order_relaxed)) break;

			m_messageRouter->ProcessControlCommands();
		}
	}
}
