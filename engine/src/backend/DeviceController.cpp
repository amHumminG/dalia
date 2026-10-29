#include "DeviceController.h"

#include <future>

#include "async/messaging/AsyncControlMessaging.h"
#include "async/messaging/AsyncMessageBus.h"

namespace dalia {
	DeviceController::~DeviceController() {
		Shutdown();
	}

	Result DeviceController::Initialize(AsyncMessageBus* asyncMessageBus, AsyncMixerSystem* mixerSystem,
		uint32_t outSampleRate, uint32_t periodSizeInFrames, const std::string& outDeviceId) {

		// Setup OS specific device manager
#ifdef _WIN32
		m_audioBackend = std::make_unique<WasapiAudioBackend>();
#else
		DALIA_LOG_CRIT(LOG_CTX_API, "Failed to initialize engine. Unsupported OS.");
		return Result::SystemError;
#endif
		Result res = m_audioBackend->Initialize();
		if (res != Result::Ok) return res;

		m_outSampleRate = outSampleRate;
		m_periodSizeFrames = periodSizeInFrames;

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

		m_asyncMessageBus = asyncMessageBus;

		m_asyncMessageBus->SubscribeToControlEvent(AsyncControlEvent::Type::OutputDeviceSwapped,
			[this](const AsyncControlEvent& ev){ FinalizeOutputDeviceSwap(ev); });

		m_asyncMessageBus->SubscribeToControlCommand(AsyncControlCommand::Type::SwapOutputDevice,
			[this](const AsyncControlCommand& cmd) { ExecuteOutputDeviceSwap(cmd); });

		m_mixerSystem = mixerSystem;

		return Result::Ok;
	}

	void DeviceController::Shutdown() {
		if (m_outputDevice)		m_outputDevice->Stop();
		if (m_nullOutputDevice)	m_nullOutputDevice->Stop();
	}

	void DeviceController::Update() {
		CheckForOutputDeviceChanges();
	}

	uint32_t DeviceController::GetOutputDeviceCount() {
		if (m_audioBackend) m_cachedOutputDeviceList = m_audioBackend->Enumerate(); // Refresh cache
		return static_cast<uint32_t>(m_cachedOutputDeviceList.size());
	}

	Result DeviceController::GetOutputDeviceInfo(uint32_t index, OutputDeviceInfo& info) {
		if (index >= m_cachedOutputDeviceList.size()) return Result::InvalidArgs;

		info = m_cachedOutputDeviceList[index];
		return Result::Ok;
	}

	Result DeviceController::GetActiveOutputDeviceInfo(OutputDeviceInfo& info) {
		if (!m_activeOutputDevice) return Result::StateCorrupted;

		snprintf(info.name, MAX_STR_LEN_DEVICE, "%s", m_activeOutputDevice->GetName().c_str());
		snprintf(info.identifier, MAX_STR_LEN_DEVICE, "%s", m_activeOutputDevice->GetIdentifier().c_str());
		info.isDefault = m_activeOutputDeviceIsDefault;
		return Result::Ok;
	}

	Result DeviceController::GetTargetOutputDeviceInfo(char* identifier, size_t maxLength) {
		if (!identifier || maxLength == 0) return Result::InvalidArgs;

		snprintf(identifier, maxLength, "%s", m_targetOutputDeviceId.c_str());
		return Result::Ok;
	}

	Result DeviceController::SetOutputDevice(const char* identifier) {
		const char* target = (identifier && identifier[0]) != '\0' ? identifier : "default"; // emtpy/nullptr is default
		if (m_targetOutputDeviceId == target) return Result::Ok;

		m_targetOutputDeviceId = target;
		m_pendingOutputDeviceSwap = true;

		DALIA_LOG_DEBUG(LOG_CTX_API, "Set output device (id: %s).", identifier);

		return Result::Ok;
	}

	Result DeviceController::StartActiveOutputDevice(AsyncMixerSystem* mixerSystem) {
		return m_activeOutputDevice->Start(mixerSystem);
	}

	SpeakerLayout DeviceController::GetActiveOutputSpeakerLayout() const {
		return m_speakerLayout;
	}

	uint32_t DeviceController::GetActiveOutputChannels() const {
		return m_outChannels;
	}

	uint32_t DeviceController::GetOutputSampleRate() const {
		return m_outSampleRate;
	}

	uint32_t DeviceController::GetPeriodSizeFrames() const {
		return m_periodSizeFrames;
	}

	uint32_t DeviceController::GetMaxPeriodSizeSamples() const {
		return m_periodSizeFrames * CHANNELS_MAX;
	}

	void DeviceController::SetupOutputDeviceDefault() {
		m_outputDevice = m_audioBackend->CreateOutputDevice("default", m_outSampleRate);
	}

	void DeviceController::SetupOutputDeviceTargeted(const std::string& outputDeviceId) {
		m_outputDevice = m_audioBackend->CreateOutputDevice(outputDeviceId.c_str(), m_outSampleRate);
		if (!m_outputDevice) SetupOutputDeviceDefault();
	}

	void DeviceController::SetupNullOutputDevice() {
		m_nullOutputDevice = std::make_unique<NullOutputDevice>(m_outSampleRate, (m_outSampleRate * 10) / 1000); // TODO: Try this with m_periodSizeFrames
	}

	void DeviceController::CheckForOutputDeviceChanges() {
		// --- Wait For Ongoing Swap ---
		if (m_isSwappingOutputDevice) return;

		// --- Initiate Swap If Needed ---
		std::string newOsDefaultOutputId;
		bool osDefaultOutputChanged = m_audioBackend->PopDeviceChangeNotification(newOsDefaultOutputId);
		bool shouldReactToOsDefaultChanged = osDefaultOutputChanged && (m_targetOutputDeviceId == "default");
		bool manualChangePending = m_pendingOutputDeviceSwap;
		bool deviceFailed = m_activeOutputDevice && m_activeOutputDevice->HasFailed();

		// Async shield (for double swaps)
		if (shouldReactToOsDefaultChanged && m_activeOutputDevice && !deviceFailed) {
			// Does the active device's id match the new os default id?
			if (m_activeOutputDevice->GetIdentifier() == newOsDefaultOutputId) {
				// We are already using the new default device -> ignore os notification
				shouldReactToOsDefaultChanged = false;
			}
		}

		if (shouldReactToOsDefaultChanged || manualChangePending || deviceFailed) {
			m_pendingOutputDeviceSwap = false;
			m_isSwappingOutputDevice = true;

			if (deviceFailed) DALIA_LOG_INFO(LOG_CTX_API, "Active output device reported failure. Initiating swap sequence.");
			else if (manualChangePending) DALIA_LOG_INFO(LOG_CTX_API, "Output device swap requested. Initiating swap sequence.");
			else DALIA_LOG_INFO(LOG_CTX_API, "OS default output device changed. Initiating swap sequence.");

			// Release ownership of active device
			OutputDevice* deviceToKill = m_outputDevice.release();
			m_activeOutputDevice = nullptr; // Temporarily without output

			// Push swap command to async control system
			AsyncControlCommand cmd = AsyncControlCommand::SwapOutputDevice(
				deviceToKill,
				m_targetOutputDeviceId.c_str(),
				m_outSampleRate
			);
			if (!m_asyncMessageBus->PushControlCommand(cmd)) {
				// TODO: Rollback ?
			}
		}
	}

	void DeviceController::FinalizeOutputDeviceSwap(const AsyncControlEvent& ev) {
		m_outputDevice.reset(ev.data.swapOutputDevice.newDevice);

		if (ev.data.swapOutputDevice.fellBackToDefault) {
			m_targetOutputDeviceId = "default";
			DALIA_LOG_WARN(LOG_CTX_API, "Requested output device was unavailable. Fell back to OS default.");
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
			DALIA_LOG_INFO(LOG_CTX_API, "Successfully swapped output device to \"%s\". %s layout with %u channel(s).",
				m_outputDevice->GetName().c_str(), layout.c_str(), m_outChannels);
		}
		else {
			// Both the requested device and the OS default device failed
			m_activeOutputDevice = m_nullOutputDevice.get(); // Fall back to null device
			m_outChannels = m_activeOutputDevice->GetChannels();
			m_speakerLayout = m_activeOutputDevice->GetSpeakerLayout();
			m_targetOutputDeviceId = "default";

			DALIA_LOG_ERR(LOG_CTX_API, "No output devices available. Audio will render to void for now.");
		}

		m_isSwappingOutputDevice = false;
	}

	void DeviceController::ExecuteOutputDeviceSwap(const AsyncControlCommand& cmd) {
		// Teardown old device
		if (cmd.data.swapOutputDevice.oldDevice) {
			// Check the format before stopping the old device
			uint32_t oldChannels = cmd.data.swapOutputDevice.oldDevice->GetChannels();
			SpeakerLayout oldLayout = cmd.data.swapOutputDevice.oldDevice->GetSpeakerLayout();
			m_nullOutputDevice->SetOutputFormat(oldChannels, oldLayout);

			DALIA_LOG_DEBUG(LOG_CTX_BACKEND, "Stopping output device \"%s\".",
				cmd.data.swapOutputDevice.oldDevice->GetName().c_str());
			cmd.data.swapOutputDevice.oldDevice->Stop();
			// Delay deletion until null device has been started
		}

		m_nullOutputDevice->Start(m_mixerSystem); // Start the null output device while new device is setting up
		if (cmd.data.swapOutputDevice.oldDevice) delete cmd.data.swapOutputDevice.oldDevice; // Delete old device

		// Attempt to create requested output device
		std::unique_ptr<OutputDevice> newDevice = m_audioBackend->CreateOutputDevice(
			cmd.data.swapOutputDevice.targetOutputDeviceId,
			cmd.data.swapOutputDevice.sampleRate
		);

		// Fall back to default if requested device failed
		bool fellBack = false;
		if (!newDevice && std::string(cmd.data.swapOutputDevice.targetOutputDeviceId) != "default") {
			newDevice = m_audioBackend->CreateOutputDevice("default", cmd.data.swapOutputDevice.sampleRate);
			fellBack = true;
			if (newDevice) {
				DALIA_LOG_DEBUG(LOG_CTX_BACKEND, "Fell back to OS default output device \"%s\".", newDevice->GetName().c_str());
			}
		}

		if (newDevice) {
			m_nullOutputDevice->Stop();
			newDevice->Start(m_mixerSystem); // Start new output device (reconfigures the mixer internally)
		}
		// If new device failed, we leave null device running

		// Push new device back to main thread
		m_asyncMessageBus->PushControlEvent(AsyncControlEvent::OutputDeviceSwapped(newDevice.release(), fellBack));
	}
}
