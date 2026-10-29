#pragma once

#include "core/Logger.h"
#include "dalia/PlaybackControl.h"

namespace dalia::dsp {

	static float GetDistanceAttenuationGain(float distance, float minDistance, float maxDistance, AttenuationCurve model) {
		if (maxDistance <= minDistance) {
			if (distance <= minDistance) return 1.0f;
			if (distance > minDistance)  return 0.0f;
		}

		if (distance <= minDistance) return 1.0f;
		if (distance >= maxDistance) return 0.0f;

		float distanceFactor = (distance - minDistance) / (maxDistance - minDistance); // Factor between 0 and 1

		switch (model) {
			case AttenuationCurve::InverseSquare: {
				float inverseFactor = minDistance / distance;
				return inverseFactor * (1.0f - distanceFactor);
			}
			case AttenuationCurve::Linear: {
				return 1.0f - distanceFactor;
			}
			case AttenuationCurve::Quadratic: {
				float inverseFactor = 1.0f - distanceFactor;
				return inverseFactor * inverseFactor;
			}
			default: return 0.0f;
		}
	}

	static void VBAP(
	CoordinateSystem coordinateSystem,
	math::Vector3 sourcePos,
	math::Vector3 listenerPos,
	math::Vector3 listenerForward,
	math::Vector3 listenerUp,
	const VirtualSpeaker* speakerMatrix,
	uint32_t spatialSpeakerCount,
	float out[CHANNELS_MAX]) {
		// Convert to listener space
		math::Vector3 relative = sourcePos - listenerPos;
		math::Vector3 listenerRight;
		if (coordinateSystem == CoordinateSystem::RightHanded) {
			listenerRight = math::Vector3::CrossProduct(listenerForward, listenerUp);
		}
		else {
			listenerRight = math::Vector3::CrossProduct(listenerUp, listenerForward);
		}


		float x = relative.Dot(listenerRight);
		float y = relative.Dot(listenerForward);

		math::Vector3 listenerSpaceDir(x, y, 0.0f);
		listenerSpaceDir.Normalize();

		for (uint32_t c = 0; c < CHANNELS_MAX; c++) out[c] = 0.0f; // Clear output

		// --- Routing Logic ---

		if (spatialSpeakerCount == 1) {
			// Mono -> Cannot apply VBAP to mono output
			out[0] = 1.0f;
		}
		else if (spatialSpeakerCount == 2) {
			// Stereo (we assume that speaker matrix [0] is left and [1] is right
			float pan = (listenerSpaceDir.x + 1.0f) * 0.5f;
			float leftSqr = 1.0f - pan;
			float rightSqr = pan;

			uint32_t leftChannel = speakerMatrix[0].channelIndex;
			uint32_t rightChannel = speakerMatrix[1].channelIndex;

			out[leftChannel] = leftSqr * math::CalculateInvSqrt(leftSqr);
			out[rightChannel] = rightSqr * math::CalculateInvSqrt(rightSqr);
		}
		else {
			// TODO: Implement VBAP for 5.1 and 7.1 surround when we need to support that
			// For now: (this should never happen)
			DALIA_LOG_ERR(LOG_CTX_MIXER, "Failed to apply spatial panning to playback. Speaker layout not supported.");
		}
	}
}
