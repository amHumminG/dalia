#include "dalia/Engine.h"

#include "engine/EngineInternalState.h"
#include "backend/DeviceManager.h"

namespace dalia {

	Result Engine::GetOutputDeviceCount(uint32_t& count) const {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;

		m_state->cachedOutputDeviceList = m_state->deviceManager->Enumerate(); // Refresh cache
		count = static_cast<uint32_t>(m_state->cachedOutputDeviceList.size());

		return Result::Ok;
	}

	Result Engine::GetOutputDeviceInfo(uint32_t index, OutputDeviceInfo& info) const {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;
		if (index >= m_state->cachedOutputDeviceList.size()) return Result::InvalidArgs;
		DALIA_LOG_DEBUG(LOG_CTX_API, "Acquired output device info (index: %u).", index);

		info = m_state->cachedOutputDeviceList[index];

		return Result::Ok;
	}

	Result Engine::GetActiveOutputDeviceInfo(OutputDeviceInfo& info) const {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;
		if (!m_state->activeOutputDevice) return Result::StateCorrupted;

		snprintf(info.name, MAX_STR_LEN_DEVICE, "%s", m_state->activeOutputDevice->GetName().c_str());
		snprintf(info.identifier, MAX_STR_LEN_DEVICE, "%s", m_state->activeOutputDevice->GetIdentifier().c_str());
		info.isDefault = m_state->activeOutputDeviceIsDefault;

		return Result::Ok;
	}

	Result Engine::GetTargetOutputDeviceId(char* identifier, size_t maxLength) const {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;
		if (!identifier || maxLength == 0) return Result::InvalidArgs;

		snprintf(identifier, maxLength, "%s", m_state->targetOutputDeviceId.c_str());

		return Result::Ok;
	}

	Result Engine::SetOutputDevice(const char* identifier) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;

		const char* target = (identifier && identifier[0]) != '\0' ? identifier : "default"; // treat emtpy/nullptr as default
		if (m_state->targetOutputDeviceId == target) return Result::Ok;

		m_state->targetOutputDeviceId = target;
		m_state->pendingOutputDeviceSwap = true;

		DALIA_LOG_DEBUG(LOG_CTX_API, "Set output device (id: %s).", identifier);

		return Result::Ok;
	}
}
