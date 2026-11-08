#include "dalia/Engine.h"

#include "engine/EngineInternalState.h"

namespace dalia {

	Result Engine::GetOutputDeviceCount(uint32_t& count) const {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;
		count = m_state->deviceController->GetOutputDeviceCount();
		return Result::Ok;
	}

	Result Engine::GetOutputDeviceInfo(uint32_t index, OutputDeviceInfo& info) const {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;
		return m_state->deviceController->GetOutputDeviceInfo(index, info);
	}

	Result Engine::GetActiveOutputDeviceInfo(OutputDeviceInfo& info) const {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;
		return m_state->deviceController->GetActiveOutputDeviceInfo(info);
	}

	Result Engine::GetTargetOutputDeviceId(char* identifier, size_t maxLength) const {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;
		return m_state->deviceController->GetTargetOutputDeviceInfo(identifier, maxLength);
	}

	Result Engine::SetOutputDevice(const char* identifier) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;
		return m_state->deviceController->SetOutputDevice(identifier);
	}
}
