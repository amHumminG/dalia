#pragma once

#include "dalia/core/Result.h"
#include "mixer/Speakers.h"

#include <string>
#include <cstdint>

namespace dalia {

	class RtSystem;

	class AudioDevice {
	public:
		virtual ~AudioDevice() = default;

		virtual Result Start(RtSystem* system) = 0;
		virtual void Stop() = 0;

		virtual const std::string& GetName() const = 0;
		virtual uint32_t GetChannelCount() const = 0;
		virtual SpeakerLayout GetSpeakerLayout() const = 0;

	};
}