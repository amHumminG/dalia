#pragma once
#include <atomic>
#include <memory>
#include <bit>

namespace placeholder_name {

	template<typename T>
	class SPSCRingBuffer {
	public:
		SPSCRingBuffer(size_t capacity) 
			: m_capacity(std::bit_ceil(capacity)), m_mask(m_capacity - 1) {
			// Ensure capacity is power of 2 so that we can use bitwise modulo (faster)
			m_buffer = std::make_unique<T[]>(m_capacity);
		}

		SPSCRingBuffer(const SPSCRingBuffer&) = delete;
		SPSCRingBuffer& operator=(const SPSCRingBuffer) = delete;

		SPSCRingBuffer(SPSCRingBuffer&&) noexcept = delete;
		SPSCRingBuffer& operator=(SPSCRingBuffer&&) noexcept = delete;

		size_t Capacity() const { return m_capacity; }

		bool Push(const T& item) {
			const size_t currentPush = m_pushCursor.load(std::memory_order_relaxed);
			const size_t nextPush = (currentPush + 1) & (m_mask);

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
			const size_t nextPop = (currentPop + 1) & (m_mask);
			m_popCursor.store(nextPop, std::memory_order_release);
			return true;
		}

	private:
		std::unique_ptr<T[]> m_buffer;
		const size_t m_capacity;
		const size_t m_mask;

		// Alignas is used here to prevent false sharing (avoiding cache misses)
		alignas(64) std::atomic<size_t> m_pushCursor{ 0 };
		alignas(64) std::atomic<size_t> m_popCursor{ 0 };
	};
}