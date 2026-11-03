#include "dalia/Engine.h"
#include "engine/EngineInternalState.h"

namespace dalia {

	Result Engine::SetGlobalDopplerFactor(float globalDopplerFactor) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;
		m_state->mixController->SetGlobalDopplerFactor(globalDopplerFactor);
		return Result::Ok;
	}
}
