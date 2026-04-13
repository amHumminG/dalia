#pragma once

#include <atomic>

template <typename T>
struct ParameterBridge {
	T buffers[2];
	std::atomic<uint32_t> writeIndex;
	std::atomic<bool> isDirty;

	ParameterBridge() : writeIndex(0), isDirty(false) {}

	void PushUpdate(const T& newParams) {
		uint32_t nextWrite = (writeIndex.load(std::memory_order_relaxed) + 1);
		buffers[nextWrite] = newParams;
		writeIndex.store(nextWrite, std::memory_order_release);
		isDirty.store(true, std::memory_order_release);
	}

	bool ConsumeUpdate(T& outParams) {
		if (!isDirty.exchange(false, std::memory_order_relaxed)) return false;
		outParams = buffers[writeIndex.load(std::memory_order_relaxed)];
		return true;
	}
};