#include "dsp/Slew.h"

#include "core/Constants.h"
#include "core/Math.h"

namespace dalia::dsp {

	void SlewFloat::SetTarget(float newTarget) {
		target = newTarget;
	}

	void SlewFloat::SnapToTarget() {
		current = target;
	}

	bool SlewFloat::Process() {
		if (math::NearlyEqual(current, target, EPSILON_DSP)) return false;

		// RC Exponential Decay curve
		float diff = target - current;
		if (std::abs(diff) < 0.001f) current = target;
		else current += diff * alpha;

		return true;
	}
}


