#include "dalia/Engine.h"
#include "engine/EngineInternalState.h"

#include <algorithm>

namespace dalia {

	Result Engine::SetGlobalDopplerFactor(float globalDopplerFactor) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;

		float clampedGlobalDopplerFactor = std::clamp(globalDopplerFactor, DOPPLER_FACTOR_MIN, DOPPLER_FACTOR_MAX);
		m_state->mixerCommands->Enqueue(AsyncMixerCommand::SetGlobalDopplerFactor(clampedGlobalDopplerFactor));

		return Result::Ok;
	}
}
