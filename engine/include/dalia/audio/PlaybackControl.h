#pragma once
#include "dalia/core/Result.h"
#include <cstdint>
#include <functional>

namespace dalia {

	struct PlaybackHandle {
	public:
		bool IsValid() const { return uuid != 0; }

		// Could add a hash function for this too?
		uint64_t GetUUID() const { return uuid; }

		bool operator==(const PlaybackHandle& other) const { return uuid == other.uuid; }
		bool operator!=(const PlaybackHandle& other) const { return uuid != other.uuid; }

	private:
		friend class Engine;
		friend struct EngineInternalState;

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

	enum class PlaybackExitCondition : uint8_t {
		NaturalEnd		= 0,
		ExplicitStop	= 1,
		Evicted			= 2,
		Error			= 3,
	};
	using AudioEventCallback = std::function<void(PlaybackHandle handle, PlaybackExitCondition exitCondition)>;

	constexpr uint32_t INVALID_REQUEST_ID = 0;
	using AssetLoadCallback = std::function<void(uint32_t requestId, Result result)>;
}