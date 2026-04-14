#pragma once

#include "core/Math.h"

namespace dalia {

	enum class SpeakerLayout {
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