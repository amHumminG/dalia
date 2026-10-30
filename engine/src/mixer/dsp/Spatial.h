#pragma once

#include "core/Logger.h"
#include "dalia/PlaybackControl.h"
#include "core/Constants.h"
#include "core/Math.h"
#include "mixer/Speakers.h"

namespace dalia::dsp {

	static float GetDistanceAttenuationGain(float distance, float minDistance, float maxDistance,
		AttenuationCurve model);

	static void VBAP(
		CoordinateSystem coordinateSystem,
		math::Vector3 sourcePos,
		math::Vector3 listenerPos,
		math::Vector3 listenerForward,
		math::Vector3 listenerUp,
		const VirtualSpeaker* speakerMatrix,
		uint32_t spatialSpeakerCount,
		float out[CHANNELS_MAX]);
}
