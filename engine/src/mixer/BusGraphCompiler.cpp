#include "mixer/BusGraphCompiler.h"
#include "core/FixedStack.h"
#include "mixer/Bus.h"

namespace dalia {

    BusGraphCompiler::BusGraphCompiler(uint32_t busCapacity)
	    : m_busCapacity(busCapacity) {
        m_pendingChildrenCount	= std::make_unique<uint32_t[]>(busCapacity);
        m_leaves		        = std::make_unique<FixedStack<uint32_t>>(busCapacity);
    }

    BusGraphCompiler::~BusGraphCompiler() = default;

    uint32_t BusGraphCompiler::Compile(std::span<const BusMirror> busPoolMirror, std::span<uint32_t> outputMixOrder) {
		uint32_t activeBusCount = 0; // For cycle detection

		// Clear pre-allocated containers
		std::fill_n(m_pendingChildrenCount.get(), m_busCapacity, 0);
		m_leaves->Clear();

		// Calculate the number of children each bus has
		for (uint32_t i = 0; i < m_busCapacity; i++) {
			if (busPoolMirror[i].refCount > 0) {
				activeBusCount++;
				uint32_t parentIndex = busPoolMirror[i].parentBusIndex;
				if (parentIndex != NO_PARENT && busPoolMirror[parentIndex].refCount > 0) {
					m_pendingChildrenCount[parentIndex]++;
				}
			}
		}

		// Find the buses with no children (leaves)
		for (uint32_t i = 0; i < m_busCapacity; i++) {
			if (busPoolMirror[i].refCount > 0 && m_pendingChildrenCount[i] == 0) {
				m_leaves->Push(i);
			}
		}

    	// Kahn's algorithm (process from leaves to root)
    	uint32_t sortedIndex = 0;
    	uint32_t currentBusIndex = 0;

		while (m_leaves->Pop(currentBusIndex)) {

			// Add it to execution list
			outputMixOrder[sortedIndex] = currentBusIndex;
			sortedIndex++;

			// Tell the parent that one of its children is sorted
			uint32_t parentIndex = busPoolMirror[currentBusIndex].parentBusIndex;
			if (parentIndex != NO_PARENT && busPoolMirror[parentIndex].refCount > 0) {
				m_pendingChildrenCount[parentIndex]--;

				// If the parent has no more pending children, it is ready to be processed
				if (m_pendingChildrenCount[parentIndex] == 0) {
					m_leaves->Push(parentIndex);
				}
			}
		}

		// Cycle exists -> Invalid graph (this should never happen!)
		if (sortedIndex != activeBusCount) {
			return 0;
		}

		return sortedIndex;
    }
}
