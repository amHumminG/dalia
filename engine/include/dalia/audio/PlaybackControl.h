#pragma once
#include <cstdint>

namespace dalia {

	struct PlaybackHandle {
	public:
		bool IsValid() const { return uuid != 0; }

		// Could add a hash function for this too?

		bool operator==(const PlaybackHandle& other) const { return uuid == other.uuid; }
		bool operator!=(const PlaybackHandle& other) const { return uuid != other.uuid; }

		friend class Engine;

	private:
		static PlaybackHandle Create(uint32_t index, uint32_t generation) {
			PlaybackHandle handle;
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

	// TODO: Have AudioEventCallback contain an enum communicating the reason for the sound finish
	// E.g. Finished naturally, stop was called on it, or engine killed it and so on...
	using AudioEventCallback = void(*)(PlaybackHandle handle, void* userData);
}