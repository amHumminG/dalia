#pragma once
#include <atomic>
#include <memory>
#include <bit>

namespace placeholder_name {

	template<typename T>
	class MPSCRingBuffer {
	public:
		struct Slot {
			T data;
			std::atomic<bool> ready{ false };
		};

		MPSCRingBuffer(size_t capacity)
			: m_capacity(std::bit_ceil(capacity + 1)), m_mask(m_capacity - 1) {
			m_buffer = std::make_unique<Slot[]>(m_capacity);
		}

		MPSCRingBuffer(const MPSCRingBuffer&) = delete;
		MPSCRingBuffer& operator=(const MPSCRingBuffer) = delete;

		MPSCRingBuffer(MPSCRingBuffer&&) noexcept = delete;
		MPSCRingBuffer& operator=(MPSCRingBuffer&&) noexcept = delete;

		bool Push(const T& item) {
			size_t currentPush;
			size_t nextPush;

			do {
				currentPush = m_pushCursor.load(std::memory_order_relaxed);
				nextPush = (currentPush + 1) & m_mask;

				if (nextPush == m_popCursor.load(std::memory_order_acquire)) {
					return false;
				}
			} while (!m_pushCursor.compare_exchange_weak(currentPush, nextPush,
				std::memory_order_release, std::memory_order_relaxed));

			m_buffer[currentPush].data = item;

			m_buffer[currentPush].ready.store(true, std::memory_order_release);
			return true;
		}

		bool Pop(T& item) {
			const size_t currentPop = m_popCursor.load(std::memory_order_relaxed);

			if (currentPop == m_pushCursor.load(std::memory_order_acquire)) {
				// Buffer is empty
				return false;
			}

			if (!m_buffer[currentPop].ready.load(std::memory_order_acquire)) {
				// A producer is currently writing to this slot
				return false
			}

			outItem = m_buffer[currentPop].data;

			m_buffer[currentPop].ready.store(false, std::memory_order_relaxed);
			m_popCursor.store((currentPop + 1) & m_mask, std::memory_order_release);
			return true;
		}

	private:
		std::unique_ptr<Slot[]> m_buffer;
		const size_t m_capacity;
		const size_t m_mask;

		alignas(64) std::atomic<size_t> m_pushCursor{ 0 };
		alignas(64) std::atomic<size_t> m_popCursor{ 0 };
	};
}