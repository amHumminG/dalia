#include "dalia/Engine.h"
#include "engine/EngineInternalState.h"

namespace dalia {

	Result Engine::SetListenerActive(uint32_t listenerIndex, bool active) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;

		if (active) {
			return m_state->ModifyListenerParams(listenerIndex, [&](ListenerParams& params) {
				params.isActive = true;
				DALIA_LOG_DEBUG(LOG_CTX_API, "Set listener %u active.", listenerIndex);
			});
		}
		return m_state->ModifyListenerParams(listenerIndex, [&](ListenerParams& params) {
			params.isActive = false;
			DALIA_LOG_DEBUG(LOG_CTX_API, "Set listener %u inactive.", listenerIndex);
		});
	}

	Result Engine::SetListener3DAttributes(uint32_t listenerIndex, const Listener3DAttributes& attributes) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;

		return m_state->ModifyListenerParams(listenerIndex, [&](ListenerParams& params) {
			params.position					= math::Vector3::FromPublic(attributes.position);
			params.distanceProbePosition	= math::Vector3::FromPublic(attributes.distanceProbePosition);
			params.forward					= math::Vector3::Normalize(math::Vector3::FromPublic(attributes.forward));
			params.up						= math::Vector3::Normalize(math::Vector3::FromPublic(attributes.up));
			params.velocity					= math::Vector3::FromPublic(attributes.velocity);
		});
	}

	Result Engine::SetListenerPosition(uint32_t listenerIndex, const Vec3& position) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;

		return m_state->ModifyListenerParams(listenerIndex, [&](ListenerParams& params) {
			params.position = math::Vector3::FromPublic(position);
		});
	}

	Result Engine::SetListenerDistanceProbePosition(uint32_t listenerIndex, const Vec3& distanceProbePosition) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;

		return m_state->ModifyListenerParams(listenerIndex, [&](ListenerParams& params) {
			params.distanceProbePosition = math::Vector3::FromPublic(distanceProbePosition);
		});
	}

	Result Engine::SetListenerOrientation(uint32_t listenerIndex, const Vec3& forward, const Vec3& up) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;

		return m_state->ModifyListenerParams(listenerIndex, [&](ListenerParams& params) {
			params.forward	= math::Vector3::Normalize(math::Vector3::FromPublic(forward));
			params.up		= math::Vector3::Normalize(math::Vector3::FromPublic(up));
		});
	}

	Result Engine::SetListenerVelocity(uint32_t listenerIndex, const Vec3& velocity) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;

		return m_state->ModifyListenerParams(listenerIndex, [&](ListenerParams& params) {
			params.velocity = math::Vector3::FromPublic(velocity);
		});
	}
}
