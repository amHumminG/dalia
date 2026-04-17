#pragma once

#include <cstdint>
#include <functional>

namespace dalia {

	/// @brief Handle used to manage playback instances. This handle will expire once the playback instance it is
	/// referencing stops.
	struct PlaybackHandle {
	public:
		/// @return true if the handle has referenced an active playback instance at some point. Otherwise, false.
		bool IsValid() const { return rawId != 0; }

		bool operator==(const PlaybackHandle& other) const { return rawId == other.rawId; }
		bool operator!=(const PlaybackHandle& other) const { return rawId != other.rawId; }

		/// @return The underlying raw id of the handle.
		uint64_t GetRawId() const { return rawId; }

	private:
		friend class Engine;
		friend struct EngineInternalState;

		static PlaybackHandle Create(uint32_t index, uint32_t generation) {
			PlaybackHandle handle;
			handle.rawId = (static_cast<uint64_t>(generation) << 32) | index;
			return handle;
		}

		uint32_t GetIndex() const { return static_cast<uint32_t>(rawId & 0xFFFFFFFF); }
		uint32_t GetGeneration() const { return static_cast<uint32_t>(rawId >> 32); }

		uint64_t rawId = 0;
	};

	constexpr PlaybackHandle InvalidPlaybackHandle{};

	/// @brief The condition under which a playback instance was stopped.
	enum class PlaybackExitCondition : uint8_t {
		NaturalEnd		= 0, // Finished naturally.
		ExplicitStop	= 1, // Explicitly stopped by an API call.
		Evicted			= 2, // Stopped to make room for a playback instance with higher priority.
		Error			= 3, // Stopped by the engine due to an error.
	};

	/// @brief A function that, if provided when creating a playback instance, will be called when the playback
	/// instance is stopped.
	using PlaybackExitCallback = std::function<void(PlaybackHandle handle, PlaybackExitCondition exitCondition)>;

	/// @brief Curve used to calculate distance attenuation.
	enum class AttenuationCurve : uint8_t {
		InverseSquare,
		Linear,
		Quadratic
	};

	/// @brief Mode used to determine what listener position to use when calculating distance attenuation.
	enum class DistanceMode {
		FromListener,		// Standard 3D (Evaluate distance from listener).
		FromDistanceProbe	// Evaluate distance from the listener's separate distance probe.
	};

	/// @brief Standard 3D vector for the DALIA API.
	struct Vec3 {
		float x = 0.0f;
		float y = 0.0f;
		float z = 0.0f;
	};

	/// @brief Transform representing a listener's position and orientation.
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

	/// @brief Routing mask used to target a specific listener. Can be combined to target multiple listeners using
	/// bitwise OR.
	using ListenerMask = uint32_t;

	constexpr ListenerMask MASK_NONE = 0b00000000;			 // Targets none of the listeners (silent if spatial).
	constexpr ListenerMask MASK_LISTENER_0 = 0b00000001;	 // Targets listener 0.
	constexpr ListenerMask MASK_LISTENER_1 = 0b00000010;	 // Targets listener 1.
	constexpr ListenerMask MASK_LISTENER_2 = 0b00000100;	 // Targets listener 2.
	constexpr ListenerMask MASK_LISTENER_3 = 0b00001000;	 // Targets listener 3.
	constexpr ListenerMask MASK_ALL_LISTENERS = 0xFFFFFFFFF; // Targets all listeners.

	/// @brief Creates a routing mask targeting a specific listener.
	///
	/// @Note[Combining masks] Multiple masks can be combined using bitwise OR.
	///
	/// @param[in] listenerIndex The zero-based index of the listener to target.
	///
	/// @return A bitmask with the corresponding listener's bit enabled.
	inline constexpr ListenerMask MakeListenerMask(uint32_t listenerIndex) { return (1 << listenerIndex); }

}