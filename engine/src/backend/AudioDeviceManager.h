#pragma once

#include "dalia/core/Result.h"
#include "backend/AudioDevice.h"

#include <string>
#include <vector>
#include <memory>

namespace dalia {

	struct AudioDeviceInfo {
		std::string name;
		std::string identifier;
		bool isDefault = false;
	};

	class AudioDeviceManager {
	public:
		virtual ~AudioDeviceManager() = default;

		virtual Result Initialize() = 0;
		virtual std::vector<AudioDeviceInfo> Enumerate() = 0;
		virtual bool HasDeviceChanged() const = 0;
		virtual void ClearDeviceChangedFlag() = 0;

		virtual std::unique_ptr<AudioDevice> CreateDevice(const std::string& identifier) = 0;
		virtual std::unique_ptr<AudioDevice> CreateNullDevice(uint32_t targetSampleRate, uint32_t periodSizeInFrames, uint32_t channelCount) = 0;

	};
}
