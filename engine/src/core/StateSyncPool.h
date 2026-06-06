#pragma once

#include "core/FixedStack.h"
#include "core/ParameterBridge.h"

#include <cstdint>
#include <memory>
#include <span>

namespace dalia {

	template<typename T, typename TMirror, typename TParams>
	class StateSyncPool {
	public:
		StateSyncPool(uint32_t capacity)
			: m_capacity(capacity), m_freeList(capacity) {
			m_pool = std::make_unique<T[]>(capacity);
			m_mirrorPool = std::make_unique<TMirror[]>(capacity);
			m_paramBridgePool = std::make_unique<ParameterBridge<TParams>[]>(capacity);

			for (int i = capacity - 1; i >= 0; i--) m_freeList.Push(i);
		}

		bool Allocate(uint32_t& outIndex) { return m_freeList.Pop(outIndex); }
		void Free(uint32_t index) { m_freeList.Push(index); }

		T& Get(uint32_t index) { return m_pool[index]; }
		TMirror& GetMirror(uint32_t index) { return m_mirrorPool[index]; }
		ParameterBridge<TParams>& GetParamBridge(uint32_t index) { return m_paramBridgePool[index]; }

		std::span<T> GetSpan() {
			return std::span<T>(m_pool.get(), m_capacity);
		}
		std::span<ParameterBridge<TParams>> GetParamBridgeSpan() {
			return std::span<ParameterBridge<TParams>>(m_paramBridgePool.get(), m_capacity);
		}

	private:
		uint32_t m_capacity;
		std::unique_ptr<T[]> m_pool;
		std::unique_ptr<TMirror[]> m_mirrorPool;
		std::unique_ptr<ParameterBridge<TParams>[]> m_paramBridgePool;
		dalia::FixedStack<uint32_t> m_freeList;

	};
}
