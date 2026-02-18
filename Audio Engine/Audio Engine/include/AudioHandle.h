#pragma once
#include <cstdint>

namespace placeholder_name {

	struct AudioHandle {
	public:
		bool IsValid() const { return uuid != 0; }

		// Could add a hash function for this too

		bool operator==(const AudioHandle& other) const { return uuid == other.uuid; }
		bool operator!=(const AudioHandle& other) const { return uuid != other.uuid; }

		friend class AudioEngine;

	private:
		static AudioHandle Create(uint32_t index, uint32_t generation) {
			AudioHandle handle;
			handle.uuid = (static_cast<uint64_t>(generation) << 32) | index;
			return handle;
		}

		uint32_t GetIndex() const {
			return static_cast<uint32_t>(uuid & 0xFFFFFFFF);
		}

		uint32_t GetGeneration() const {
			return static_cast<uint32_t>(uuid >> 32);
		}

		uint64_t uuid = 0;
	};
}