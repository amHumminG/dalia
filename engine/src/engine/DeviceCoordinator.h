#pragma once

#include "backend/DeviceManager.h"
#include "backend/Windows/WindowsDeviceManager.h"
#include "backend/OutputDevice.h"
#include "backend/NullOutputDevice.h"
#include "core/Logger.h"
#include "core/FixedQueue.h"

#include <memory>



namespace dalia {

	static constexpr size_t DEVICE_COORDINATOR_CONTROL_EVENT_STACK_SIZE = 64;

	struct AsyncControlEvent;

	class DeviceCoordinator {
	public:
		DeviceCoordinator();
		~DeviceCoordinator();

		// TODO: Write doxygen comments

		Result Initialize(uint32_t outSampleRate, uint32_t periodSizeInFrames, const std::string& outDeviceId = "default");
		void Shutdown();

		void Update();

		void SubmitControlEvent(const AsyncControlEvent& ev); // TODO: Should we really do this? ...

	private:
		void SetupOutputDeviceDefault();
		void SetupOutputDeviceTargeted(const std::string& outputDeviceId);
		void SetupNullOutputDevice();

		void ProcessOutputDeviceSwappedEvent(const AsyncControlEvent& ev);

		FixedQueue<AsyncControlEvent> m_controlEvents;

		std::unique_ptr<DeviceManager> m_deviceManager;
		std::unique_ptr<OutputDevice> m_outputDevice;
		std::unique_ptr<NullOutputDevice> m_nullOutputDevice;
		OutputDevice* m_activeOutputDevice = nullptr; // Points to either outputDevice or nullDevice

		// Output device swapping
		std::vector<OutputDeviceInfo> m_cachedOutputDeviceList;
		std::string m_targetOutputDeviceId = "default";
		bool m_activeOutputDeviceIsDefault = true;
		bool m_pendingOutputDeviceSwap = false;
		bool m_isSwappingOutputDevice = false;

		// Output info
		SpeakerLayout m_speakerLayout = SpeakerLayout::Mono;
		uint32_t m_periodSizeFrames = 0;
		uint32_t m_periodSizeSamplesMax = 0;
		uint32_t m_outChannels = 0;
		uint32_t m_outSampleRate = 0;
	};



}
