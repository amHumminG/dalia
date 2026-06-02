#pragma once

#include <atomic>

#include "core/Math.h"

namespace dalia {

	struct ListenerParams {
		bool isActive = false;

		math::Vector3 position{0.0f, 0.0f, 0.0f};				// Panning/Distance attenuation
		math::Vector3 distanceProbePosition{0.0f, 0.0f, 0.0f};	// Probe position (for distance if activated)

		math::Vector3 forward{0.0f, 0.0f, 1.0f};
		math::Vector3 up{0.0f, 1.0f, 0.0f};

		math::Vector3 velocity{0.0f, 0.0f, 0.0f};
	};

	struct Listener {
		ListenerParams params;
	};

	struct ListenerMirror {
		ListenerParams params;
		bool isParamsDirty = false;
	};
}