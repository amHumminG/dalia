#pragma once

#include <cstdint>
#include <memory>
#include <span>

#include "core/SPSCRingBuffer.h"

namespace dalia {

	template <typename T>
	class AsyncPool {
	public:
		AsyncPool(uint32_t capacity)
			: m_capacity(capacity), m_freeList(capacity) {
			m_pool = std::make_unique<T[]>(capacity);

			for (int i = 0; i < capacity; i++) m_freeList.Push(i);
		}

		bool Allocate(uint32_t& outIndex) { return m_freeList.Pop(outIndex); }
		bool Free(uint32_t index) { return m_freeList.Push(index); }

		T& Get(uint32_t index) { return m_pool[index]; }

		std::span<T> GetSpan() {
			return std::span<T>(m_pool.get(), m_capacity);
		}
		SPSCRingBuffer<uint32_t>* GetFreeList() { return &m_freeList; }

	private:
		uint32_t m_capacity;
		std::unique_ptr<T[]> m_pool;
		SPSCRingBuffer<uint32_t> m_freeList;

	};
}
