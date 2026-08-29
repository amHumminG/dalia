#pragma once

#include "dalia/audio/DeviceControl.h"
#include "dalia/core/Result.h"
#include "backend/OutputDevice.h"

#include <string>
#include <vector>
#include <memory>

namespace dalia {

	class DeviceManager {
	public:
		virtual ~DeviceManager() = default;

		virtual Result Initialize() = 0;

		virtual std::vector<OutputDeviceInfo> Enumerate() = 0;
		virtual bool PollDefaultOutputDeviceChanged(std::string& newDeviceId) = 0;

		virtual std::unique_ptr<OutputDevice> CreateDevice(const char* identifier, uint32_t engineSampleRate) = 0;
		virtual std::unique_ptr<OutputDevice> CreateNullDevice(uint32_t engineSampleRate, uint32_t periodSizeInFrames) = 0;

	};
}
