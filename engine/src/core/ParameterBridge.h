#pragma once

#include <atomic>
#include <cstdint>

template <typename T>
struct ParameterBridge {
	struct SharedState {
		uint32_t index = 1;
		bool isDirty = false;
	};

	T buffers[3];
	std::atomic<SharedState> sharedState{ SharedState{.index = 1, .isDirty = false} };

	uint32_t writeIndex = 2; // Owned by producer
	uint32_t readIndex = 0;	 // Owned by consumer

	void PushUpdate(const T& newParams) {
		buffers[writeIndex] = newParams;

		SharedState newState{.index = writeIndex, .isDirty = true};
		SharedState oldState = sharedState.exchange(newState, std::memory_order_acq_rel);
		writeIndex = oldState.index;
	}

	bool ConsumeUpdate(T& outParams) {
		if (!sharedState.load(std::memory_order_acquire).isDirty) return false;

		SharedState newState{.index = readIndex, .isDirty = false};
		SharedState oldState = sharedState.exchange(newState, std::memory_order_acq_rel);
		readIndex = oldState.index;

		outParams = buffers[readIndex];
		return true;
	}
};