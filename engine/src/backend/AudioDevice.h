#pragma once

#include "dalia/core/Result.h"

namespace dalia {

	class RtSystem;

	class AudioDevice {
		virtual ~AudioDevice() = default;

		virtual Result Start(RtSystem* system) = 0;
		virtual void Stop() = 0;
	};
}