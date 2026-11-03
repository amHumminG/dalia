#pragma once

#include "core/FixedStack.h"
#include "core/ParameterBridge.h"

#include <cstdint>
#include <memory>
#include <span>
#include <cassert>

namespace dalia {

	template<typename T, typename TParams>
	struct MixerPoolView {
		size_t capacity = 0;
		std::span<T> nodes;
		std::span<ParameterBridge<TParams>> bridges;

		void SyncParams() {
			TParams params;
			for (size_t i = 0; i < bridges.size(); i++) {
				if (bridges[i].ConsumeUpdate(params)) {
					nodes[i].SetParams(params);
				}
			}
		}
	};

	template<typename TMirror, typename TParams>
	concept StateSyncedMirror = requires(TMirror m) {
		{ m.params } -> std::same_as<TParams&>;
	};

	/// @brief A pool of items that need to be synced between the main thread and the mixer thread.
	/// @tparam T The item on the mixer thread.
	/// @tparam TMirror The item on the main thread.
	/// @tparam TParams The parameters of the item contained in both T and TMirror
	template<typename T, typename TMirror, typename TParams>
	requires StateSyncedMirror<TMirror, TParams>
	class StateSyncPool {
	public:
		explicit StateSyncPool(uint32_t capacity)
			: m_capacity(capacity),
			m_pool(std::make_unique<T[]>(capacity)),
			m_mirrorPool(std::make_unique<TMirror[]>(capacity)),
			m_paramBridgePool(std::make_unique<ParameterBridge<TParams>[]>(capacity)),
			m_dirtyFlags(std::make_unique<bool[]>(capacity)),
			m_freeList(capacity) {

			for (uint32_t i = capacity; i > 0; --i) m_freeList.Push(i - 1);
		}

		/// @brief Allocates an item in the pool.
		/// @param outIndex The index that the item was allocated at.
		/// @return True if the allocation was successful, otherwise false.
		bool Allocate(uint32_t& outIndex) { return m_freeList.Pop(outIndex); }

		/// @brief Frees an item in the pool.
		/// @param index The index of the item to free.
		void Free(uint32_t index) {
			assert(index < m_capacity && "StateSyncPool::Free index out of bounds.");
			m_dirtyFlags[index] = false;
			m_freeList.Push(index);
		}

		/// @brief Retrieves the capacity of the pool.
		[[nodiscard]] uint32_t Capacity() const { return m_capacity; }
		T& Get(uint32_t index) const {
			assert(index < m_capacity && "StateSyncPool::Get index out of bounds.");
			return m_pool[index];
		}

		/// @brief Retrieves the mirror at the given index.
		/// @param index The index of the pool item.
		/// @return A reference to the mirror inside the pool.
		TMirror& GetMirror(uint32_t index) const {
			assert(index < m_capacity && "StateSyncPool::GetMirror index out of bounds.");
			return m_mirrorPool[index];
		}

		/// @brief Retrieves the parameter bridge of an item in the pool.
		/// @param index The index of the pool item.
		/// @return A reference to the parameter bridge inside the pool.
		ParameterBridge<TParams>& GetParamBridge(uint32_t index) const {
			assert(index < m_capacity && "StateSyncPool::GetParamBridge index out of bounds.");
			return m_paramBridgePool[index];
		}

		/// @brief Retrieves the mirror parameters for modification. This marks the parameters as dirty internally.
		/// @param index The index of the pool item.
		/// @return A reference to the mirror parameters to be modified.
		TParams& GetMirrorParamsForWrite(uint32_t index) {
			assert(index < m_capacity && "StateSyncPool::GetParamsForWrite index out of bounds.");
			m_dirtyFlags[index] = true;
			return m_mirrorPool[index].params;
		}

		/// @brief Marks the parameters of a pool item as dirty.
		/// @param index The index of the pool item.
		void MarkDirty(uint32_t index) {
			assert(index < m_capacity && "StateSyncPool::MarkDirty index out of bounds.");
			m_dirtyFlags[index] = true;
		}

		/// @brief Constructs a span of the internal item pool.
		/// @return The span of items.
		std::span<T> GetSpan() const {
			return std::span<T>(m_pool.get(), m_capacity);
		}

		/// @brief Constructs a span from the internal parameter bridge pool.
		/// @return The span of parameter bridges.
		std::span<ParameterBridge<TParams>> GetParamBridgeSpan() const {
			return std::span<ParameterBridge<TParams>>(m_paramBridgePool.get(), m_capacity);
		}

		MixerPoolView<T, TParams> GetMixerView() {
			return {
				m_capacity,
				std::span<T>(m_pool.get(), m_capacity),
				std::span<ParameterBridge<TParams>>(m_paramBridgePool.get(), m_capacity)
			};
		}

		/// @brief Syncs all dirty pool items across the main thread and the audio thread.
		void Sync() {
			for (uint32_t index = 0; index < m_capacity; index++) {
				if (!m_dirtyFlags[index]) continue;

				m_paramBridgePool[index].PushUpdate(m_mirrorPool[index].params);
				m_dirtyFlags[index] = false;
			}
		}

	private:
		uint32_t m_capacity; // The capacity of the pools.
		std::unique_ptr<T[]> m_pool; // The pool of audio thread items.
		std::unique_ptr<TMirror[]> m_mirrorPool; // The pool of main thread items.
		std::unique_ptr<ParameterBridge<TParams>[]> m_paramBridgePool; // The bridges that links the main thread and the audio thread.
		std::unique_ptr<bool[]> m_dirtyFlags; // Flags that keeps track of which mirrors need to be re-synced.
		FixedStack<uint32_t> m_freeList; // Keeps track of all free slots in the pool.
	};


}
