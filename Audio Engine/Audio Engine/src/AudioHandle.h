#pragma once

namespace placeholder_name {

	struct AudioHandle {
		uint64_t uuid = 0;

		bool IsValid() const { return uuid != 0; }

		uint32_t GetIndex() const {
			return static_cast<uint32_t>(uuid & 0xFFFFFFFF);
		}
		
		uint32_t GetGeneration() const {
			return static_cast<uint32_t>(uuid >> 32);
		}

		bool operator==(const AudioHandle& other) const { return uuid == other.uuid; }
		bool operator!=(const AudioHandle& other) const { return uuid != other.uuid; }
	};
}