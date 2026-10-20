#include "dalia/Engine.h"
#include "engine/EngineInternalState.h"
#include "core/Logger.h"

#include <algorithm>

namespace dalia {

	Result Engine::CreateBus(const char* identifier, const char* parentIdentifier) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;

		uint32_t bIndex;
		uint32_t bIndexParent = NO_PARENT;
		bool parentFound = false;

		// Fetch parent bus
		Result resParent = m_state->GetBusIndex(parentIdentifier, bIndexParent);
		if (resParent == Result::Ok) parentFound = true;

		// Fetch bus
		Result res = m_state->GetBusIndex(identifier, bIndex);
		if (res == Result::Ok) {
			BusMirror& bMirror  = m_state->buses.GetMirror(bIndex);
			bMirror.refCount++;

			if (parentFound && bMirror.parentBusIndex != bIndexParent) {
				DALIA_LOG_WARN(LOG_CTX_API,
					"CreateBus called for existing bus (%s) with conflicting parent. Ignoring new parent.", identifier);
			}
			return Result::Ok;
		}

		if (!parentFound) {
			DALIA_LOG_ERR(LOG_CTX_API, "Failed to create bus %s (routed -> %s). No bus with identifier %s exists.",
				identifier, parentIdentifier, parentIdentifier);
			return resParent;
		}

		// Bus does not exist yet
		if (!m_state->buses.Allocate(bIndex)) {
			DALIA_LOG_ERR(LOG_CTX_API, "Failed to create bus (%s). Bus pool capacity reached.", identifier);
			return Result::BusCapacityReached;
		}

		BusMirror& bMirror = m_state->buses.GetMirror(bIndex);
		bMirror.refCount = 1;
		bMirror.parentBusIndex = bIndexParent;

		const BusID bId(identifier);
		m_state->busMap[bId] = bIndex;

		DALIA_LOG_DEBUG(LOG_CTX_API, "Created bus with routing: %s (index: %d) -> %s (index: %d).",
			identifier, bIndex, parentIdentifier, bIndexParent);
		m_state->mixerCommands->Enqueue(AsyncMixerCommand::AllocateBus(bIndex, bIndexParent));

		return Result::Ok;
	}

	Result Engine::DestroyBus(const char* identifier) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;

		auto it = m_state->busMap.find(BusID(identifier));
		if (it == m_state->busMap.end()) {
			DALIA_LOG_ERR(LOG_CTX_API, "Failed to destroy bus (%s). Bus does not exist.", identifier);
			return Result::InvalidBus;
		}
		uint32_t bIndex = it->second;

		if (bIndex == MASTER_BUS_INDEX) {
			DALIA_LOG_ERR(LOG_CTX_API, "Failed to destroy bus (%s). Master cannot be destroyed.", identifier);
			return Result::Error;
		}

		BusMirror& bMirror = m_state->buses.GetMirror(bIndex);
		if (bMirror.refCount > 1) {
			bMirror.refCount--;
			return Result::Ok;
		}

		// Reference count is 1 -> Destroy bus

		// Remove parent from child buses
		uint32_t orphanedBuses = 0;
		for (uint32_t i = 0; i < m_state->buses.Capacity(); i++) {
			BusMirror* bMirrorChild = &m_state->buses.GetMirror(i);
			if (bMirrorChild->parentBusIndex == bIndex) {
				bMirrorChild->parentBusIndex = NO_PARENT;
				m_state->mixerCommands->Enqueue(AsyncMixerCommand::SetBusParent(i, NO_PARENT));
				orphanedBuses++;
				DALIA_LOG_DEBUG(LOG_CTX_API, "Orphaned bus (index: %d).", i);
			}
		}
		if (orphanedBuses > 0) DALIA_LOG_WARN(LOG_CTX_API, "Destroying bus %s. Orphaned %d buses.",
			identifier, orphanedBuses);

		uint32_t orphanedPlaybacks = 0;
		for (uint32_t i = 0; i < m_state->voices.Capacity(); i++) {
			VoiceMirror& vMirrorChild = m_state->voices.GetMirror(i);
			if (vMirrorChild.parentBusIndex == bIndex) {
				vMirrorChild.parentBusIndex = NO_PARENT;
				m_state->mixerCommands->Enqueue(AsyncMixerCommand::SetVoiceParent(i, vMirrorChild.gen, NO_PARENT));
				orphanedPlaybacks++;
				DALIA_LOG_DEBUG(LOG_CTX_API, "Orphaned voice (index: %d).", i);
			}
		}
		if (orphanedPlaybacks > 0) DALIA_LOG_WARN(LOG_CTX_API, "Destroying bus %s. Orphaned %d playbacks.",
			identifier, orphanedPlaybacks);

		bMirror.Reset();
		m_state->busMap.erase(it);
		m_state->buses.Free(bIndex);

		DALIA_LOG_DEBUG(LOG_CTX_API, "Destroyed bus %s (index: %d).", identifier, bIndex);
		m_state->mixerCommands->Enqueue(AsyncMixerCommand::FreeBus(bIndex));

		return Result::Ok;
	}

	Result Engine::RouteBus(const char* identifier, const char* parentIdentifier) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;

		// Fetch bus
		uint32_t bIndex;
		Result res = m_state->GetBusIndex(identifier, bIndex);
		if (res != Result::Ok) {
			DALIA_LOG_ERR(LOG_CTX_API, "Failed to route bus (%s -> %s). No bus with identifier %s exists.",
				identifier, parentIdentifier, identifier);
			return res;
		}

		if (bIndex == MASTER_BUS_INDEX) {
			DALIA_LOG_ERR(LOG_CTX_API, "Failed to route bus (%s -> %s). Master cannot be routed.",
				identifier, parentIdentifier);
			return Result::InvalidRouting;
		}

		// Fetch parent bus
		uint32_t bIndexParent;
		Result resParent = m_state->GetBusIndex(parentIdentifier, bIndexParent);
		if (resParent != Result::Ok) {
			DALIA_LOG_ERR(LOG_CTX_API, "Failed to route bus (%s -> %s). No bus with identifier %s exists.",
				identifier, parentIdentifier, parentIdentifier);
			return resParent;
		}

		// Cycle detection
		uint32_t currentAncestor = bIndexParent;
		while (currentAncestor != NO_PARENT) {
			if (currentAncestor == bIndex) {
				DALIA_LOG_ERR(LOG_CTX_API, "Failed to route bus (%s -> %s). Bus graph cycle detected.",
					identifier, parentIdentifier);
				return Result::InvalidRouting;
			}
			currentAncestor = m_state->buses.GetMirror(currentAncestor).parentBusIndex;
		}

		BusMirror& bMirror = m_state->buses.GetMirror(bIndex);
		if (bMirror.parentBusIndex == bIndexParent) {
			DALIA_LOG_WARN(LOG_CTX_API, "Attempting to route bus (%s -> %s). %s is already routed to %s.",
				identifier, parentIdentifier, identifier, parentIdentifier);
			return Result::Ok;
		}
		bMirror.parentBusIndex = bIndexParent;

		DALIA_LOG_DEBUG(LOG_CTX_API, "Routed bus %s (index: %d) -> %s (index: %d).",
			identifier, bIndex, parentIdentifier, bIndexParent);
		m_state->mixerCommands->Enqueue(AsyncMixerCommand::SetBusParent(bIndex, bIndexParent));

		return Result::Ok;
	}

	Result Engine::SetBusVolumeDb(const char* identifier, float volumeDb) {
		if (!EngineInternalState::IsInitialized(m_state)) return Result::NotInitialized;

		return m_state->ModifyBusParams(identifier, [&](BusParams& params) {
			params.gain = math::DbToGain(std::clamp(volumeDb, VOLUME_DB_MIN, VOLUME_DB_MAX));
		});
	}
}
