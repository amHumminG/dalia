#pragma once
#include <atomic>
#include <array>

template<typename T, size_t Capacity>
class SPSCRingBuffer {
	// Ensure capacity is power of 2 so that we can use bitwise modulo (faster)
	static_assert((Capacity& (Capacity - 1)) == 0, "Capacity must be power of 2");

public:
	static constexpr size_t Capacity() const { return Capacity };

	bool Push(const T& item) {
		const size_t currentPush = m_pushCursor.load(std::memory_order_relaxed);
		const size_t nextPush = (currentPush + 1) & (Capacity - 1);

		if (nextPush == m_popCursor.load(std::memory_order_acquire)) {
			// Buffer is full
			return false;
		}

		m_buffer[currentPush] = item;
		m_pushCursor.store(nextPush, std::memory_order_release);
		return true;
	}

	bool Pop(T& item) {
		const size_t currentPop = m_popCursor.load(std::memory_order_relaxed);

		if (currentPop == m_pushCursor.load(std::memory_order_acquire)) {
			// Buffer is empty
			return false;
		}

		item = m_buffer[currentPop];
		const size_t nextPop = (currentPop + 1) & (Capacity - 1);
		m_popCursor.store(nextPop, std::memory_order_release);
		return true;
	}

private:
	std::array<T, Capacity> m_buffer;

	// Alignas is used here to prevent false sharing (avoiding cache misses)
	alignas(64) std::atomic<size_t> m_pushCursor{ 0 };
	alignas(64) std::atomic<size_t> m_popCursor{ 0 };
};