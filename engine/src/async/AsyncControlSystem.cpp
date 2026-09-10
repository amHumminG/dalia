#include "async/AsyncControlSystem.h"

#include "backend/windows/WindowsDeviceManager.h"
#include "core/Logger.h"
#include "mixer/RtSystem.h"

namespace dalia {

	AsyncControlSystem::AsyncControlSystem(const AsyncControlSystemConfig& config)
		: m_requestQueue(config.requestQueue),
		m_eventQueue(config.eventQueue),
		m_deviceManager(config.deviceManager),
		m_nullOutputDevice(config.nullOutputDevice),
		m_rtSystem(config.rtSystem) {}

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
		if (FAILED(hr)) DALIA_LOG_ERR(LOG_CTX_ASYNC, "Failed to initialize COM on AsyncControlThread.");

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
				// Teardown old device
				if (req.data.swapOutputDevice.oldDevice) {
					DALIA_LOG_DEBUG(LOG_CTX_BACKEND, "Stopping output device \"%s\".", req.data.swapOutputDevice.oldDevice->GetName().c_str());
					req.data.swapOutputDevice.oldDevice->Stop();
					delete req.data.swapOutputDevice.oldDevice;
				}

				m_rtSystem->SetOutputFormat(m_nullOutputDevice->GetChannelCount(), m_nullOutputDevice->GetSpeakerLayout());
				m_nullOutputDevice->Start(m_rtSystem);

				std::unique_ptr<OutputDevice> newDevice = m_deviceManager->CreateDevice(
					req.data.swapOutputDevice.targetOutputDeviceId,
					req.data.swapOutputDevice.sampleRate
				);

				// Fallback to default if requested device failed
				bool fellBack = false;
				if (!newDevice && std::string(req.data.swapOutputDevice.targetOutputDeviceId) != "default") {
					newDevice = m_deviceManager->CreateDevice("default", req.data.swapOutputDevice.sampleRate);
					fellBack = true;
					DALIA_LOG_DEBUG(LOG_CTX_BACKEND, "Fell back to OS default output device \"%s\".", newDevice->GetName().c_str());
				}

				if (newDevice) {
					m_nullOutputDevice->Stop();

					// Reconfigure mixer and start new device
					m_rtSystem->SetOutputFormat(newDevice->GetChannelCount(), newDevice->GetSpeakerLayout());
					newDevice->Start(m_rtSystem);
				}
				// If new device failed, we leave null device running

				// Push new device back
				auto ev = AsyncControlEvent::OutputDeviceSwapped(newDevice.release(), fellBack);
				m_eventQueue->Push(ev);

				break;
			}
			default: break;
		}
	}
}
