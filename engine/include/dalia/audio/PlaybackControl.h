#pragma once

#include <cstdint>
#include <functional>

namespace dalia {

	struct PlaybackHandle {
	public:
		bool IsValid() const { return uuid != 0; }

		uint32_t GetIndex() const { return static_cast<uint32_t>(uuid & 0xFFFFFFFF); }
		uint32_t GetGeneration() const { return static_cast<uint32_t>(uuid >> 32); }

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

		uint64_t uuid = 0;
	};

	enum class PlaybackExitCondition : uint8_t {
		NaturalEnd		= 0,
		ExplicitStop	= 1,
		Evicted			= 2,
		Error			= 3,
	};
	using AudioEventCallback = std::function<void(PlaybackHandle handle, PlaybackExitCondition exitCondition)>;

	enum class AttenuationModel : uint8_t {
		InverseSquare,
		Linear,
		Quadratic
	};

	enum class DistanceMode {
		FromListener,		// Standard 3D (Evaluate distance from listener)
		FromDistanceProbe	// Evaluate listener from a custom 3D position
	};

	using ListenerMask = uint32_t;
	constexpr ListenerMask MASK_ALL_LISTENERS = 0xFFFFFFFF;
	constexpr ListenerMask MASK_NONE = 0x00000000;
}