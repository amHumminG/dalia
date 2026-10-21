#pragma once
#include <memory>

namespace dalia {

	template<typename T>
	class FixedQueue {
	public:
		FixedQueue(size_t capacity)
			: m_capacity(std::bit_ceil(capacity)), m_mask(m_capacity - 1) {
			m_buffer = std::make_unique<T[]>(m_capacity);
		}

		FixedQueue(const FixedQueue&) = delete;
		FixedQueue& operator=(const FixedQueue&) = delete;

		FixedQueue(FixedQueue&&) noexcept = delete;
		FixedQueue& operator=(FixedQueue&&) noexcept = delete;

		bool Push(const T& item) {
			size_t nextPush = (m_pushCursor + 1) & (m_mask);

			if (nextPush == m_popCursor) return false; // Buffer full

			m_buffer[m_pushCursor] = item;
			m_pushCursor = nextPush;
			return true;
		}

		bool Pop(T& item) {
			if (m_popCursor == m_pushCursor) return false; // Buffer empty

			item = m_buffer[m_popCursor];
			m_popCursor = (m_popCursor + 1) & (m_mask);
			return true;
		}

		void Clear() {
			m_popCursor = 0;
			m_pushCursor = 0;
		}

		bool IsEmpty() {
			return m_popCursor == m_pushCursor;
		}

		bool IsFull() {
			return (m_pushCursor + 1) == m_popCursor;
		}

		// TODO: Implement
		// size_t Size() {
		//
		// }

		size_t Capacity() {
			return m_capacity;
		}

	private:
		std::unique_ptr<T[]> m_buffer;
		const size_t m_capacity;
		const size_t m_mask;

		// Alignas is used here to prevent false sharing (avoiding cache misses)
		alignas(64) size_t m_pushCursor = 0;
		alignas(64) size_t m_popCursor = 0;
	};
}
