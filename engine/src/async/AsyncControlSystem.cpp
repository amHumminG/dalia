#include "async/AsyncControlSystem.h"

#include "backend/windows/WindowsDeviceManager.h"
#include "backend/NullOutputDevice.h"
#include "core/Logger.h"
#include "mixer/MixerSystem.h"

namespace dalia {

	AsyncControlSystem::AsyncControlSystem(const AsyncControlSystemConfig& config)
		: m_requestQueue(config.requestQueue),
		m_eventQueue(config.eventQueue),
		m_deviceManager(config.deviceManager),
		m_nullOutputDevice(config.nullOutputDevice),
		m_mixerSystem(config.mixerSystem) {}

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
		// Initialize COM for this thread TODO: Remove this!
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
					// Check the format before stopping the old device
					uint32_t oldChannels = req.data.swapOutputDevice.oldDevice->GetChannelCount();
					SpeakerLayout oldLayout = req.data.swapOutputDevice.oldDevice->GetSpeakerLayout();
					m_nullOutputDevice->SetOutputFormat(oldChannels, oldLayout);

					DALIA_LOG_DEBUG(LOG_CTX_BACKEND, "Stopping output device \"%s\".",
						req.data.swapOutputDevice.oldDevice->GetName().c_str());
					req.data.swapOutputDevice.oldDevice->Stop();
					// Delay deletion until null device has been started
				}

				m_nullOutputDevice->Start(m_mixerSystem); // Start the null output device while new device is setting up
				if (req.data.swapOutputDevice.oldDevice) delete req.data.swapOutputDevice.oldDevice; // Delete old device

				// Attempt to create requested output device
				std::unique_ptr<OutputDevice> newDevice = m_deviceManager->CreateDevice(
					req.data.swapOutputDevice.targetOutputDeviceId,
					req.data.swapOutputDevice.sampleRate
				);

				// Fall back to default if requested device failed
				bool fellBack = false;
				if (!newDevice && std::string(req.data.swapOutputDevice.targetOutputDeviceId) != "default") {
					newDevice = m_deviceManager->CreateDevice("default", req.data.swapOutputDevice.sampleRate);
					fellBack = true;
					DALIA_LOG_DEBUG(LOG_CTX_BACKEND,
						"Fell back to OS default output device \"%s\".", newDevice->GetName().c_str());
				}

				if (newDevice) {
					m_nullOutputDevice->Stop();
					newDevice->Start(m_mixerSystem); // Start new output device (reconfigures the mixer internally)
				}
				// If new device failed, we leave null device running

				// Push new device back to main thread
				auto ev = AsyncControlEvent::OutputDeviceSwapped(newDevice.release(), fellBack);
				m_eventQueue->Push(ev);

				break;
			}
			default: break;
		}
	}
}
