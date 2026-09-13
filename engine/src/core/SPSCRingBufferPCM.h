#pragma once

#include <atomic>
#include <memory>
#include <bit>
#include <cstring>
#include <type_traits>
#include <algorithm>
#include <cassert>

namespace dalia {

	template<typename T>
	class SPSCRingBufferPCM {
		static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable for memcpy.");

	public:
		SPSCRingBufferPCM(size_t maxPhysicalSamples)
			: m_maxPhysicalSamples(maxPhysicalSamples) {
			m_buffer = std::make_unique<T[]>(m_maxPhysicalSamples);
		}

		SPSCRingBufferPCM(const SPSCRingBufferPCM&) = delete;
		SPSCRingBufferPCM& operator=(const SPSCRingBufferPCM&) = delete;
		SPSCRingBufferPCM(SPSCRingBufferPCM&&) noexcept = delete;
		SPSCRingBufferPCM& operator=(SPSCRingBufferPCM&&) noexcept = delete;

		// Clears the buffer and set a new format
		void ClearAndSetFormat(size_t logicalCapacityFrames, uint32_t channels) {
			assert(logicalCapacityFrames * channels <= m_maxPhysicalSamples);

			m_capacityFrames = std::bit_ceil(logicalCapacityFrames);
			m_frameMask = m_capacityFrames - 1;
			m_channels = channels;

			m_pushCursor.store(0, std::memory_order_release);
			m_popCursor.store(0, std::memory_order_release);
		}

		size_t CapacitySamples() const { return m_maxPhysicalSamples; }

		size_t CapacityLogicalFrames() const { return m_capacityFrames; }

		// Returns the number of frames that can be read by the consumer
		size_t GetAvailableFramesRead() const {
			const size_t push = m_pushCursor.load(std::memory_order_acquire);
			const size_t pop = m_popCursor.load(std::memory_order_relaxed);
			return push - pop;
		}

		// Returns the number of frames that can be written by the producer
		size_t GetAvailableFramesWrite() const {
			const size_t push = m_pushCursor.load(std::memory_order_relaxed);
			const size_t pop = m_popCursor.load(std::memory_order_acquire);
			return m_capacityFrames - (push - pop);
		}

		// Pushes a number of frames from the provided buffer to the ring buffer. Returns true if the call was successful
		bool PushFrames(const T* data, size_t frameCount) {
			const size_t currentPush = m_pushCursor.load(std::memory_order_relaxed);
			const size_t currentPop = m_popCursor.load(std::memory_order_acquire);

			// Is there enough space to write?
			if (frameCount > m_capacityFrames - (currentPush - currentPop)) return false;

			const size_t framePushIndex = currentPush & m_frameMask;
			const size_t framesFirstPart = std::min(frameCount, m_capacityFrames - framePushIndex);
			const size_t framesSecondPart = frameCount - framesFirstPart;

			std::memcpy(m_buffer.get() + (framePushIndex * m_channels), data, framesFirstPart * m_channels * sizeof(T));

			if (framesSecondPart > 0) {
				std::memcpy(m_buffer.get(), data + (framesFirstPart * m_channels), framesSecondPart * m_channels * sizeof(T));
			}

			m_pushCursor.store(currentPush + frameCount, std::memory_order_release);
			return true;
		}

		size_t PopFrames(T* data, size_t frameCount) {
			const size_t currentPush = m_pushCursor.load(std::memory_order_acquire);
			const size_t currentPop = m_popCursor.load(std::memory_order_relaxed);

			size_t availableFrames = currentPush - currentPop;
			size_t framesToPop = std::min(frameCount, availableFrames);

			if (framesToPop == 0) return 0;

			const size_t framePopIndex = currentPop & m_frameMask;
			const size_t framesFirstPart = std::min(framesToPop, m_capacityFrames - framePopIndex);
			const size_t framesSecondPart = framesToPop - framesFirstPart;

			std::memcpy(data, m_buffer.get() + (framePopIndex * m_channels), framesFirstPart * m_channels * sizeof(T));
			if (framesSecondPart > 0) {
				std::memcpy(data + (framesFirstPart * m_channels), m_buffer.get(), framesSecondPart * m_channels * sizeof(T));
			}

			m_popCursor.store(currentPop + framesToPop, std::memory_order_release);
			return framesToPop;
		}

		size_t DiscardFrames(size_t frameCount) {
			const size_t currentPush = m_pushCursor.load(std::memory_order_acquire);
			const size_t currentPop = m_popCursor.load(std::memory_order_relaxed);

			size_t availableFrames = currentPush - currentPop;
			size_t framesToPop = std::min(frameCount, availableFrames);

			// Is there enough data in the buffer?
			if (framesToPop > 0) {
				m_popCursor.store(currentPop + framesToPop, std::memory_order_release);
			}

			return framesToPop;
		}

	private:
		std::unique_ptr<T[]> m_buffer;
		const size_t m_maxPhysicalSamples = 0;

		size_t m_capacityFrames = 0;
		size_t m_frameMask = 0;
		size_t m_channels = 0;

		alignas(64) std::atomic<size_t> m_pushCursor{0};
		alignas(64) std::atomic<size_t> m_popCursor{0};
	};
}
