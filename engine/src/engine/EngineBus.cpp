#include "dalia/Engine.h"
#include "engine/EngineInternalState.h"

namespace dalia {

	Result Engine::CreateBus(const char* bus, const char* parentBus) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;
		return m_state->mixController->CreateBus(bus, parentBus);
	}

	Result Engine::DestroyBus(const char* bus) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;
		return m_state->mixController->DestroyBus(bus);
	}

	Result Engine::RouteBus(const char* bus, const char* parentBus) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;
		return m_state->mixController->RouteBus(bus, parentBus);
	}

	Result Engine::SetBusVolumeDb(const char* bus, float volumeDb) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;
		return m_state->mixController->SetBusVolumeDb(bus, volumeDb);
	}
}
