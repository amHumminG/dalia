#include "dalia/Engine.h"
#include "engine/EngineInternalState.h"
#include "core/Math.h"

namespace dalia {

	Result Engine::SetListenerActive(uint32_t listenerIndex, bool active) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;
		return m_state->mixController->SetListenerActive(listenerIndex, active);
	}

	Result Engine::SetListenerPosition(uint32_t listenerIndex, const Vec3& position) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;
		return m_state->mixController->SetListenerPosition(listenerIndex, math::Vector3::FromPublic(position));
	}

	Result Engine::SetListenerDistanceProbePosition(uint32_t listenerIndex, const Vec3& position) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;
		return m_state->mixController->SetListenerDistanceProbePosition(listenerIndex,
			math::Vector3::FromPublic(position));
	}

	Result Engine::SetListenerVelocity(uint32_t listenerIndex, const Vec3& velocity) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;
		return m_state->mixController->SetListenerVelocity(listenerIndex, math::Vector3::FromPublic(velocity));
	}

	Result Engine::SetListenerOrientation(uint32_t listenerIndex, const Vec3& forward, const Vec3& up) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;
		return m_state->mixController->SetListenerOrientation(listenerIndex, math::Vector3::FromPublic(forward),
			math::Vector3::FromPublic(up));
	}

	Result Engine::SetListener3DAttributes(uint32_t listenerIndex, const Listener3DAttributes& attributes) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;
		return m_state->mixController->SetListener3DAttributes(listenerIndex,
			math::Vector3::FromPublic(attributes.position),
			math::Vector3::FromPublic(attributes.distanceProbePosition),
			math::Vector3::FromPublic(attributes.forward),
			math::Vector3::FromPublic(attributes.up),
			math::Vector3::FromPublic(attributes.velocity)
		);
	}
}
