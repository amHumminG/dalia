#pragma once

#include "dalia/Result.h"
#include "mixer/Speakers.h"

#include <string>
#include <cstdint>

namespace dalia {

	class Mixer;

	class OutputDevice {
	public:
		virtual ~OutputDevice() = default;

		virtual Result Start(Mixer* mixer) = 0;
		virtual void Stop() = 0;

		[[nodiscard]] virtual bool HasFailed() const = 0;

		[[nodiscard]] virtual const std::string& GetIdentifier() const = 0;
		[[nodiscard]] virtual const std::string& GetName() const = 0;
		[[nodiscard]] virtual uint32_t GetChannels() const = 0;
		[[nodiscard]] virtual SpeakerLayout GetSpeakerLayout() const = 0;
	};
}