#pragma once

/// @file Result.hpp
/// @brief Return codes and related helpers.

namespace dalia {

	/// @brief Return codes for the DALIA API.
	///
	/// Represents the success or failure of an API call. Values < 0 represent failures.
	// clang-format off
	enum class Result : int {
		Ok							= 0,	///< Ok!

		// Generic Errors
		Error						= -1,	///< Unspecified or general failure
		NotInitialized				= -2,	///< Engine or subsystem has not been initialized
		StateCorrupted				= -3,	///< Internal engine state is corrupted
		AlreadyInitialized			= -4,	///< Initialization was called on an already initialized instance
		InvalidArgs					= -5,	///< One or more arguments are null, out of range, or malformed
		InvalidHandle				= -6,	///< Handle is null or malformed
		ExpiredHandle				= -7,	///< Handle points to a slot that has already been destroyed or recycled
		InvalidBus					= -8,	///< Target bus does not exist
		InvalidRouting				= -9,	///< Bus routing connection is invalid or would create a cycle
		InvalidEffectSlot			= -10,	///< Effect slot index is out of range or unavailable
		InvalidListener				= -11,	///< Target listener does not exist
		SoundNotLoaded				= -12,	///< Sound asset has not finished loading or failed to load

		// Pool Capacity Errors
		CapacityReached				= -100,	///< Generic fixed-size pool capacity limit reached
		SoundCapacityReached		= -101,	///< Maximum number of loaded sound assets reached (either resident or streaming)
		VoiceCapacityReached		= -102, ///< Maximum number of concurrent playback voices reached
		StreamCapacityReached		= -103,	///< Maximum number of active audio streams reached
		BusCapacityReached			= -104, ///< Maximum number of buses reached
		EffectCapacityReached		= -105, ///< Maximum number of active DSP effects reached

		// Messaging Errors
		MixerQueueCapacityReached	= -200,	///< Mixer-thread command/event queue is full
		StreamQueueCapacityReached	= -201,	///< Stream-thread command queue is full
		LoadQueueCapacityReached	= -202,	///< Load-thread command/event queue is full
		ControlQueueCapacityReached = -203, ///< Control-thread command/event queue is full

		// Playback Errors
		PlaybackCorrupted			= -300,	///< Playback voice state is corrupted

		// I/O Errors
		SoundLoadError				= -400,	///< General failure while loading/decoding a sound from a file
		FileReadError				= -401, ///< Failed to open or read audio data from disk
		UnsupportedFormat			= -402, ///< Audio file uses a format that is not supported

		// Backed Errors
		SystemError					= -500, ///< Platform audio system error
		DeviceNotFound				= -501, ///< Requested or default audio output device was not found
		DeviceFailed				= -502, ///< Audio device failed to open, start, or recover
		ClientFailed				= -503, ///< Audio backend client initialization or session failed
	};
	// clang-format on

	/// @brief Converts a Result code into a readable string literal.
	///
	/// @param result The Result code to evaluate.
	///
	/// @return A null-terminated string describing the result.
	constexpr const char* GetResultString(const Result result) {
		switch (result) {
			case Result::Ok:
				return "Ok";

			case Result::Error:
				return "Error (generic)";
			case Result::NotInitialized:
				return "Engine is not initialized.";
			case Result::StateCorrupted:
				return "Engine state is corrupted.";
			case Result::AlreadyInitialized:
				return "Engine is already initialized.";
			case Result::InvalidArgs:
				return "Invalid arguments.";
			case Result::InvalidHandle:
				return "Invalid handle";
			case Result::ExpiredHandle:
				return "Expired handle";
			case Result::InvalidBus:
				return "Bus not found";
			case Result::InvalidRouting:
				return "Invalid routing";
			case Result::InvalidEffectSlot:
				return "Invalid effect slot";
			case Result::InvalidListener:
				return "Listener not found";

			case Result::CapacityReached:
				return "Pool capacity reached (generic)";
			case Result::SoundCapacityReached:
				return "Resident/Stream sound pool capacity reached";
			case Result::VoiceCapacityReached:
				return "Voice pool capacity reached";
			case Result::StreamCapacityReached:
				return "Stream pool capacity reached";
			case Result::BusCapacityReached:
				return "Bus pool capacity reached";
			case Result::EffectCapacityReached:
				return "Effect pool capacity reached";

			case Result::MixerQueueCapacityReached:
				return "Mixer-thread queue capacity reached";
			case Result::StreamQueueCapacityReached:
				return "Stream-thread queue capacity reached";
			case Result::LoadQueueCapacityReached:
				return "Load-thread queue capacity reached";
			case Result::ControlQueueCapacityReached:
				return "Control-thread queue capacity reached";

			case Result::PlaybackCorrupted:
				return "Playback corrupted";

			case Result::SoundLoadError:
				return "Sound load error";
			case Result::FileReadError:
				return "File read error";
			case Result::UnsupportedFormat:
				return "Unsupported format";

			case Result::SystemError:
				return "System error";
			case Result::DeviceNotFound:
				return "Device not found";
			case Result::DeviceFailed:
				return "Device Failed";
			case Result::ClientFailed:
				return "Client Failed";

			default:
				return "No string is defined for this result type";
		}
	}
}
