#pragma once

#include <atomic>

#include "core/Math.h"

namespace dalia {

	struct ListenerParams {
		math::Vector3 position;
		math::Vector3 forward{0.0f, 0.0f, 1.0f};
		math::Vector3 up{0.0f, 1.0f, 0.0f};
	};

	struct Listener {
		bool isActive = false;

		ListenerParams params;
	};

	struct ListenerMirror {
		bool isActive = false;

		ListenerParams params;
		bool isParamsDirty = false;
	};
}