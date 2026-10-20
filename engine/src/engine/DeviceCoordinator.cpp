#include "engine/DeviceCoordinator.h"

#include "messaging/AsyncControlMessaging.h"

namespace dalia {
	DeviceCoordinator::DeviceCoordinator()
		: m_controlEvents(64){
	}

	Result DeviceCoordinator::Initialize(uint32_t outSampleRate, uint32_t periodSizeInFrames,
	                                     const std::string& outDeviceId) {

		// Setup OS specific device manager
#ifdef _WIN32
		m_deviceManager = std::make_unique<WindowsDeviceManager>();
#else
		DALIA_LOG_CRIT(LOG_CTX_API, "Failed to initialize engine. Unsupported OS.");
		return Result::SystemError;
#endif
		Result res = m_deviceManager->Initialize();
		if (res != Result::Ok) return res;

		m_outSampleRate = outSampleRate;
		m_periodSizeFrames = periodSizeInFrames;
		m_periodSizeSamplesMax = periodSizeInFrames * CHANNELS_MAX;

		// Set up output devices
		SetupOutputDeviceTargeted(outDeviceId);
		SetupNullOutputDevice();

		if (m_outputDevice) {
			m_activeOutputDevice = m_outputDevice.get();
			m_nullOutputDevice->SetOutputFormat(m_outputDevice->GetChannels(), m_outputDevice->GetSpeakerLayout());

			// Logging
			std::string layout;
			switch (m_speakerLayout) {
				case SpeakerLayout::Mono: layout = "Mono"; break;
				case SpeakerLayout::Stereo: layout = "Stereo"; break;
				case SpeakerLayout::Surround51: layout = "5.1"; break;
				case SpeakerLayout::Surround71: layout = "7.1"; break;
			}
			DALIA_LOG_INFO(LOG_CTX_API, "Initialized audio device \"%s\". %s layout with %u channel(s).",
				m_outputDevice->GetName().c_str(), layout.c_str(), m_outputDevice->GetChannels());
		}
		else {
			DALIA_LOG_WARN(LOG_CTX_API, "No output audio device found. Audio will render to void for now.");
			m_activeOutputDevice = m_nullOutputDevice.get();
		}

		return Result::Ok;
	}

	void DeviceCoordinator::Shutdown() {
		if (m_outputDevice)		m_outputDevice->Stop();
		if (m_nullOutputDevice)	m_nullOutputDevice->Stop();
	}

	void DeviceCoordinator::ProcessOutputDeviceSwappedEvent(const AsyncControlEvent& ev) {
		m_outputDevice.reset(ev.data.swapOutputDevice.newDevice);

		if (ev.data.swapOutputDevice.fellBackToDefault) {
			m_targetOutputDeviceId = "default";
			DALIA_LOG_WARN(LOG_CTX_API, "Requested audio output device was unavailable. Fell back to OS default.");
		}

		m_activeOutputDeviceIsDefault = (m_targetOutputDeviceId == "default");

		if (m_outputDevice) {
			m_activeOutputDevice = m_outputDevice.get();
			m_outChannels = m_activeOutputDevice->GetChannels();
			m_speakerLayout = m_activeOutputDevice->GetSpeakerLayout();

			std::string layout;
			switch (m_speakerLayout) {
				case SpeakerLayout::Mono: layout = "Mono"; break;
				case SpeakerLayout::Stereo: layout = "Stereo"; break;
				case SpeakerLayout::Surround51: layout = "5.1"; break;
				case SpeakerLayout::Surround71: layout = "7.1"; break;
			}
			DALIA_LOG_INFO(LOG_CTX_API, "Successfully swapped audio device to \"%s\". %s layout with %u channel(s).",
				m_outputDevice->GetName().c_str(), layout.c_str(), m_outChannels);
		}
		else {
			// Both the requested device and the OS default device failed
			m_activeOutputDevice = m_nullOutputDevice.get(); // Fall back to null device
			m_outChannels = m_activeOutputDevice->GetChannels();
			m_speakerLayout = m_activeOutputDevice->GetSpeakerLayout();
			m_targetOutputDeviceId = "default";

			DALIA_LOG_ERR(LOG_CTX_API, "No audio output devices available. Audio will render to void for now.");
		}

		m_isSwappingOutputDevice = false;
	}

	void DeviceCoordinator::SubmitControlEvent(const AsyncControlEvent& ev) {
		m_controlEvents.Push(ev);
	}

	void DeviceCoordinator::SetupOutputDeviceDefault() {
		m_outputDevice = m_deviceManager->CreateDevice("default", m_outSampleRate);
	}

	void DeviceCoordinator::SetupOutputDeviceTargeted(const std::string& outputDeviceId) {
		m_outputDevice = m_deviceManager->CreateDevice(outputDeviceId.c_str(), m_outSampleRate);
		if (!m_outputDevice) SetupOutputDeviceDefault();
	}

	void DeviceCoordinator::SetupNullOutputDevice() {
		m_nullOutputDevice = std::make_unique<NullOutputDevice>(m_outSampleRate, (m_outSampleRate * 10) / 1000); // TODO: Try this with m_periodSizeFrames
	}

	void DeviceCoordinator::ClaimOutputDevice(OutputDevice* newDevice) {
		m_outputDevice.reset(newDevice);
	}



	void ProcessOutputDeviceSwap(OutputDevice* newDevice, )
}
