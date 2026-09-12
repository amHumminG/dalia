#pragma once

#include <atomic>
#include <memory>
#include <bit>
#include <cstring>
#include <type_traits>
#include <algorithm>

namespace dalia {

	template<typename T>
	class SPSCRingBufferPCM {
		static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable for memcpy.");

	public:
		SPSCRingBufferPCM(size_t capacity)
			: m_capacity(std::bit_ceil(capacity)), m_mask(m_capacity - 1) {
			m_buffer = std::make_unique<T[]>(m_capacity);
		}

		SPSCRingBufferPCM(const SPSCRingBufferPCM&) = delete;
		SPSCRingBufferPCM& operator=(const SPSCRingBufferPCM&) = delete;
		SPSCRingBufferPCM(SPSCRingBufferPCM&&) noexcept = delete;
		SPSCRingBufferPCM& operator=(SPSCRingBufferPCM&&) noexcept = delete;

		size_t Capacity() const { return m_capacity; }

		// Returns the number of frames that can be read by the consumer
		size_t GetAvailableRead() const {
			const size_t push = m_pushCursor.load(std::memory_order_acquire);
			const size_t pop = m_popCursor.load(std::memory_order_relaxed);
			return push - pop;
		}

		size_t GetAvailableWrite() const {
			const size_t push = m_pushCursor.load(std::memory_order_relaxed);
			const size_t pop = m_popCursor.load(std::memory_order_acquire);
			return m_capacity - (push - pop);
		}

		bool Push(const T* data, size_t count) {
			const size_t currentPush = m_pushCursor.load(std::memory_order_relaxed);
			const size_t currentPop = m_popCursor.load(std::memory_order_acquire);

			// Is there enough space to write?
			if (count > m_capacity - (currentPush - currentPop)) return false;

			const size_t pushIndex = currentPush & m_mask;
			const size_t firstPart = std::min(count, m_capacity - pushIndex);
			const size_t secondPart = count - firstPart;

			std::memcpy(m_buffer.get() + pushIndex, data, firstPart * sizeof(T));

			if (secondPart > 0) std::memcpy(m_buffer.get(), data + firstPart, secondPart * sizeof(T));

			m_pushCursor.store(currentPush + count, std::memory_order_release);
			return true;
		}

		// Pops a block of data. Returns false if it was unable to provide the requested count
		bool Pop(T* data, size_t count) {
			const size_t currentPush = m_pushCursor.load(std::memory_order_acquire);
			const size_t currentPop = m_popCursor.load(std::memory_order_relaxed);

			// Is there enough data in the buffer?
			if (count > (currentPush - currentPop)) return false;

			const size_t popIndex = currentPop & m_mask;
			const size_t firstPart = std::min(count, m_capacity - popIndex);
			const size_t secondPart = count - firstPart;

			std::memcpy(data, m_buffer.get() + popIndex, firstPart * sizeof(T));
			if (secondPart > 0) std::memcpy(data + firstPart, m_buffer.get(), secondPart * sizeof(T));

			m_pushCursor.store(currentPush + count, std::memory_order_release);
			return true;
		}

		bool PopDiscard(size_t count) {
			const size_t currentPush = m_pushCursor.load(std::memory_order_acquire);
			const size_t currentPop = m_popCursor.load(std::memory_order_relaxed);

			// Is there enough data in the buffer?
			if (count > (currentPush - currentPop)) return false;

			m_popCursor.store(currentPop + count, std::memory_order_release);
		}

	private:
		std::unique_ptr<T[]> m_buffer;
		const size_t m_capacity;
		const size_t m_mask;

		alignas(64) std::atomic<size_t> m_pushCursor{0};
		alignas(64) std::atomic<size_t> m_popCursor{0};
	};
}
