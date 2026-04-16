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

	// User-facing vector struct used for API methods
	struct Vec3 {
		float x = 0.0f;
		float y = 0.0f;
		float z = 0.0f;
	};

	struct ListenerTransform {
		Vec3 position;
		Vec3 distanceProbePosition;
		Vec3 forward;
		Vec3 up;
	};

	/// @brief Constructs a listener transform for a listener that performs panning and attenuation logic from the same
	///	world position.
	///
	/// @param[in] position The world position of the listener.
	/// @param[in] forward	The direction the listener is facing.
	/// @param[in] up		The up direction from the listeners position.
	///
	/// @return The listener transform.
	inline constexpr ListenerTransform MakeListenerTransform(const Vec3& position, const Vec3& forward, const Vec3& up) {
		return { position, position, forward, up };
	}

	/// @brief Constructs a listener transform for a listener that performs panning and attenuation logic from two
	/// separate positions.
	///
	/// @param[in] position			The world position that the listener will use for panning logic.
	/// @param[in] probePosition	The world position that the listener will use for attenuation logic.
	/// @param[in] forward			The direction the listener is facing.
	/// @param[in] up				The up direction from the listeners position.
	///
	/// @return The listener transform.
	inline constexpr ListenerTransform MakeListenerTransformSplit(const Vec3& position, const Vec3& probePosition,
	                                                              const Vec3& forward, const Vec3& up) {
		return { position, probePosition, forward, up };
	}

	using ListenerMask = uint32_t;
	constexpr ListenerMask MASK_ALL_LISTENERS = 0xFFFFFFFF;
	constexpr ListenerMask MASK_NONE = 0x00000000;

	/// @brief Creates a routing mask targeting a specific listener.
	///
	/// @Note[Combining masks] Multiple masks can be combined using bitwise OR (|).
	///
	/// @param[in] listenerIndex The zero-based index of the listener to target.
	///
	/// @return A bitmask with the corresponding listener's bit enabled.
	inline constexpr ListenerMask MakeListenerMask(uint32_t listenerIndex) { return (1 << listenerIndex); }

}