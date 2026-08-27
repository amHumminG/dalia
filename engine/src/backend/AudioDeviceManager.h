#pragma once

#include "dalia/audio/DeviceControl.h"
#include "dalia/core/Result.h"
#include "backend/AudioDevice.h"

#include <string>
#include <vector>
#include <memory>

namespace dalia {

	class AudioDeviceManager {
	public:
		virtual ~AudioDeviceManager() = default;

		virtual Result Initialize() = 0;

		virtual std::vector<AudioDeviceInfo> Enumerate() = 0;
		virtual bool PollDeviceChanged() = 0;

		virtual std::unique_ptr<AudioDevice> CreateDevice(const char* identifier, uint32_t engineSampleRate) = 0;
		virtual std::unique_ptr<AudioDevice> CreateNullDevice(uint32_t engineSampleRate, uint32_t periodSizeInFrames) = 0;

	};
}
