#include "dalia/Engine.hpp"
#include "engine/EngineInternalState.hpp"

namespace dalia {

	Result Engine::SetGlobalDopplerFactor(float globalDopplerFactor) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;
		m_state->mixController->SetGlobalDopplerFactor(globalDopplerFactor);
		return Result::Ok;
	}
}
