#include "mixer/MixController.h"

#include <algorithm>

namespace dalia {

		Result MixController::CreateBus(const char* bus, const char* parentBus) {
		uint32_t bIndex;
		uint32_t bIndexParent = NO_PARENT;
		bool parentFound = false;

		// Fetch parent bus
		Result resParent = GetBusIndex(parentBus, bIndexParent);
		if (resParent == Result::Ok) parentFound = true;

		// Fetch bus
		Result res = GetBusIndex(bus, bIndex);
		if (res == Result::Ok) {
			BusMirror& bMirror  = m_buses.GetMirror(bIndex);
			bMirror.refCount++;

			if (parentFound && bMirror.parentBusIndex != bIndexParent) {
				DALIA_LOG_WARN(LOG_CTX_CORE,
					"CreateBus called for existing bus (%s) with conflicting parent. Ignoring new parent.", bus);
			}
			return Result::Ok;
		}

		if (!parentFound) {
			DALIA_LOG_ERR(LOG_CTX_CORE, "Failed to create bus %s (routed -> %s). No bus with identifier %s exists.",
				bus, parentBus, parentBus);
			return resParent;
		}

		// Bus does not exist yet
		if (!m_buses.Allocate(bIndex)) {
			DALIA_LOG_ERR(LOG_CTX_CORE, "Failed to create bus (%s). Bus pool capacity reached.", bus);
			return Result::BusCapacityReached;
		}

		BusMirror& bMirror = m_buses.GetMirror(bIndex);
		bMirror.refCount = 1;
		bMirror.parentBusIndex = bIndexParent;

		const BusID bId(bus);
		m_busMap[bId] = bIndex;

		DALIA_LOG_DEBUG(LOG_CTX_CORE, "Created bus with routing: %s (index: %d) -> %s (index: %d).",
			bus, bIndex, parentBus, bIndexParent);
		m_asyncMessageBus->PushMixerCommand(AsyncMixerCommand::AllocateBus(bIndex, bIndexParent));

		return Result::Ok;
	}

	Result MixController::DestroyBus(const char* bus) {
		auto it = m_busMap.find(BusID(bus));
		if (it == m_busMap.end()) {
			DALIA_LOG_ERR(LOG_CTX_CORE, "Failed to destroy bus (%s). Bus does not exist.", bus);
			return Result::InvalidBus;
		}
		uint32_t bIndex = it->second;

		if (bIndex == MASTER_BUS_INDEX) {
			DALIA_LOG_ERR(LOG_CTX_CORE, "Failed to destroy bus (%s). Master cannot be destroyed.", bus);
			return Result::Error;
		}

		BusMirror& bMirror = m_buses.GetMirror(bIndex);
		if (bMirror.refCount > 1) {
			bMirror.refCount--;
			return Result::Ok;
		}

		// Reference count is 1 -> Destroy bus

		// Remove parent from child buses
		uint32_t orphanedBuses = 0;
		for (uint32_t i = 0; i < m_buses.Capacity(); i++) {
			BusMirror* bMirrorChild = &m_buses.GetMirror(i);
			if (bMirrorChild->parentBusIndex == bIndex) {
				bMirrorChild->parentBusIndex = NO_PARENT;
				m_asyncMessageBus->PushMixerCommand(AsyncMixerCommand::SetBusParent(i, NO_PARENT));
				orphanedBuses++;
				DALIA_LOG_DEBUG(LOG_CTX_CORE, "Orphaned bus (index: %d).", i);
			}
		}
		if (orphanedBuses > 0) DALIA_LOG_WARN(LOG_CTX_CORE, "Destroying bus %s. Orphaned %d buses.",
			bus, orphanedBuses);

		uint32_t orphanedPlaybacks = 0;
		for (uint32_t i = 0; i < m_voices.Capacity(); i++) {
			VoiceMirror& vMirrorChild = m_voices.GetMirror(i);
			if (vMirrorChild.parentBusIndex == bIndex) {
				vMirrorChild.parentBusIndex = NO_PARENT;
				m_asyncMessageBus->PushMixerCommand(AsyncMixerCommand::SetVoiceParent(
					i, vMirrorChild.gen, NO_PARENT
				));
				orphanedPlaybacks++;
				DALIA_LOG_DEBUG(LOG_CTX_API, "Orphaned voice (index: %d).", i);
			}
		}
		if (orphanedPlaybacks > 0) DALIA_LOG_WARN(LOG_CTX_CORE, "Destroying bus %s. Orphaned %d playbacks.",
			bus, orphanedPlaybacks);

		bMirror.Reset();
		m_busMap.erase(it);
		m_buses.Free(bIndex);

		DALIA_LOG_DEBUG(LOG_CTX_CORE, "Destroyed bus %s (index: %d).", bus, bIndex);
		m_asyncMessageBus->PushMixerCommand(AsyncMixerCommand::FreeBus(bIndex));

		return Result::Ok;
	}

	Result MixController::RouteBus(const char* bus, const char* parentBus) {
		// Fetch bus
		uint32_t bIndex;
		Result res = GetBusIndex(bus, bIndex);
		if (res != Result::Ok) {
			DALIA_LOG_ERR(LOG_CTX_CORE, "Failed to route bus (%s -> %s). No bus with identifier %s exists.",
				bus, parentBus, bus);
			return res;
		}

		if (bIndex == MASTER_BUS_INDEX) {
			DALIA_LOG_ERR(LOG_CTX_CORE, "Failed to route bus (%s -> %s). Master cannot be routed.",
				bus, parentBus);
			return Result::InvalidRouting;
		}

		// Fetch parent bus
		uint32_t bIndexParent;
		Result resParent = GetBusIndex(parentBus, bIndexParent);
		if (resParent != Result::Ok) {
			DALIA_LOG_ERR(LOG_CTX_CORE, "Failed to route bus (%s -> %s). No bus with identifier %s exists.",
				bus, parentBus, parentBus);
			return resParent;
		}

		if (WouldCreateBusCycle(bIndex, bIndexParent)) return Result::InvalidRouting;

		BusMirror& bMirror = m_buses.GetMirror(bIndex);
		if (bMirror.parentBusIndex == bIndexParent) {
			DALIA_LOG_WARN(LOG_CTX_CORE, "Attempting to route bus (%s -> %s). %s is already routed to %s.",
				bus, parentBus, bus, parentBus);
			return Result::Ok;
		}
		bMirror.parentBusIndex = bIndexParent;

		DALIA_LOG_DEBUG(LOG_CTX_CORE, "Routed bus %s (index: %d) -> %s (index: %d).",
			bus, bIndex, parentBus, bIndexParent);
		m_asyncMessageBus->PushMixerCommand(AsyncMixerCommand::SetBusParent(bIndex, bIndexParent));

		return Result::Ok;
	}

	Result MixController::SetBusVolumeDb(const char* bus, float volumeDb) {
		return ModifyBusParams(bus, [&](BusParams& params) {
			params.gain = math::DbToGain(std::clamp(volumeDb, VOLUME_DB_MIN, VOLUME_DB_MAX));
		});
	}
}
