#include "async/AsyncControlSystem.h"

#include "backend/windows/WindowsDeviceManager.h"

#include "core/SPSCRingBuffer.h"
#include "messaging/AsyncControlMessaging.h"

#include <objbase.h>

#include "core/Logger.h"

namespace dalia {

	AsyncControlSystem::AsyncControlSystem(const AsyncControlSystemConfig& config)
		: m_requestQueue(config.requestQueue), m_eventQueue(config.eventQueue), m_deviceManager(config.deviceManager) {
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
		// Initialize COM for this thread
		HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
		if (FAILED(hr)) DALIA_LOG_ERR(LOG_CTX_CONTROL, "Failed to initialize COM on AsyncControlThread.");

		while (m_isRunning.load(std::memory_order_relaxed)) {
			m_taskSemaphore.acquire(); // Sleep (wake on notification)

			if (!m_isRunning.load(std::memory_order_relaxed)) break;

			AsyncControlRequest req;
			while (m_requestQueue->Pop(req)) ProcessRequest(req);
		}

		if (SUCCEEDED(hr)) CoUninitialize();
	}

	void AsyncControlSystem::ProcessRequest(const AsyncControlRequest& req) {
		switch (req.type) {
			case AsyncControlRequest::Type::SwapOutputDevice: {
				std::unique_ptr<OutputDevice> newDevice = m_deviceManager->CreateDevice(
					req.data.swapOutputDevice.targetOutputDeviceId,
					req.data.swapOutputDevice.sampleRate
				);

				// Fallback to default if requested device failed
				bool fellBack = false;
				if (!newDevice && std::string(req.data.swapOutputDevice.targetOutputDeviceId) != "default") {
					newDevice = m_deviceManager->CreateDevice("default", req.data.swapOutputDevice.sampleRate);
					fellBack = true;
				}

				// Push new device back
				auto ev = AsyncControlEvent::OutputDeviceSwapped(newDevice.release(), fellBack);
				m_eventQueue->Push(ev);

				break;
			}
			default: break;
		}
	}
}
