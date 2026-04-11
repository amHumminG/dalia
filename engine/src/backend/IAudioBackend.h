#pragma once

#include "dalia/core/Result.h"

#include <cstdint>

namespace dalia {

	class RtSystem;

	class IAudioBackend {
	public:
		virtual ~IAudioBackend() = default;

		virtual Result Init() = 0;
		virtual Result Start() = 0;
		virtual void Stop() = 0;

		virtual void AttachSystem(RtSystem* system) = 0;

		virtual uint32_t GetSampleRate() const = 0;
		virtual uint32_t GetChannelCount() const = 0;
		virtual uint32_t GetPeriodSizeInFrames() const = 0;
		virtual uint32_t GetBufferCapacityInFrames() const = 0;
	};
}