#pragma once

#include "core/Math.hpp"

namespace dalia {

	enum class SpeakerLayout : uint8_t {
		Mono,
		Stereo,
		Surround51,
		Surround71
	};

	struct VirtualSpeaker {
		math::Vector3 direction;
		uint32_t channelIndex;
	};
}
