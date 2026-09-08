#pragma once

#include "dalia/DeviceControl.h"
#include "dalia/Result.h"
#include "backend/OutputDevice.h"

#include <string>
#include <vector>
#include <memory>

namespace dalia {

	struct OSDeviceNotification {
		char deviceId[MAX_STR_LEN_DEVICE];
	};

	class DeviceManager {
	public:
		virtual ~DeviceManager() = default;

		virtual Result Initialize() = 0;

		virtual std::vector<OutputDeviceInfo> Enumerate() = 0;
		virtual bool PopDeviceChangeNotification(std::string& newDeviceId) = 0;

		virtual std::unique_ptr<OutputDevice> CreateDevice(const char* identifier, uint32_t engineSampleRate) = 0;
		virtual std::unique_ptr<OutputDevice> CreateNullDevice(uint32_t engineSampleRate, uint32_t periodSizeInFrames) = 0;

	};
}
