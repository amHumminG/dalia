#include "mixer/MixController.h"

namespace dalia {

	Result MixController::SetListenerActive(uint32_t listenerIndex, bool active) {
		return ModifyListenerParams(listenerIndex, [&](ListenerParams& params) {
			params.isActive = active;
			if (active) DALIA_LOG_DEBUG(LOG_CTX_API, "Set listener %u active.", listenerIndex);
			else DALIA_LOG_DEBUG(LOG_CTX_API, "Set listener %u inactive.", listenerIndex);
		});
	}

	Result MixController::SetListenerPosition(uint32_t listenerIndex, math::Vector3 position) {
		return ModifyListenerParams(listenerIndex, [&](ListenerParams& params) {
			params.position = position;
		});
	}

	Result MixController::SetListenerVelocity(uint32_t listenerIndex, math::Vector3 velocity) {
		return ModifyListenerParams(listenerIndex, [&](ListenerParams& params) {
			params.velocity = velocity;
		});
	}

	Result MixController::SetListenerDistanceProbePosition(uint32_t listenerIndex, math::Vector3 position) {
		return ModifyListenerParams(listenerIndex, [&](ListenerParams& params) {
			params.distanceProbePosition = position;
		});
	}

	Result MixController::SetListenerOrientation(uint32_t listenerIndex, math::Vector3 forward, math::Vector3 up) {
		return ModifyListenerParams(listenerIndex, [&](ListenerParams& params) {
			params.forward	= math::Vector3::Normalize(forward);
			params.up		= math::Vector3::Normalize(up);
		});
	}

	Result MixController::SetListener3DAttributes(uint32_t listenerIndex, math::Vector3 position,
		math::Vector3 distanceProbePosition, math::Vector3 forward, math::Vector3 up, math::Vector3 velocity) {
		return ModifyListenerParams(listenerIndex, [&](ListenerParams& params) {
			params.position				 = position;
			params.distanceProbePosition = distanceProbePosition;
			params.forward				 = math::Vector3::Normalize(forward);
			params.up					 = math::Vector3::Normalize(up);
			params.velocity				 = velocity;
		});
	}
}
