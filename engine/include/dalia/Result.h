#pragma once

/// @file Result.h
/// @brief Return codes and related helpers.

namespace dalia {

	/// @brief Return codes for the DALIA API.
	///
	/// Represents the success or failure of an API call. Values < 0 represent failures.
	enum class Result : int {
		Ok							= 0,

		// Generic Errors
		Error						= -1,
		NotInitialized				= -2,
		StateCorrupted				= -3,
		AlreadyInitialized			= -4,
		InvalidArgs					= -5,
		InvalidHandle				= -6,
		ExpiredHandle				= -7,
		InvalidBus					= -8,
		InvalidRouting				= -9,
		InvalidEffectSlot			= -10,
		InvalidListener				= -11,

		// Pool Capacity Errors
		CapacityReached				= -100,
		SoundCapacityReached		= -101,
		VoiceCapacityReached		= -103,
		StreamCapacityReached		= -104,
		BusCapacityReached			= -105,
		EffectCapacityReached		= -106,

		// Messaging Errors
		MixerQueueCapacityReached	= -200,
		StreamQueueCapacityReached	= -201,
		LoadQueueCapacityReached	= -202,
		ControlQueueCapacityReached = -203,

		// Playback Errors
		PlaybackCorrupted			= -300,

		// I/O Errors
		SoundLoadError				= -400,
		FileReadError				= -401,
		UnsupportedFormat			= -402,

		// Backed Errors
		SystemError					= -500,
		DeviceNotFound				= -501,
		DeviceFailed				= -502,
		ClientFailed				= -503,
	};

	/// @brief Converts a Result code into a readable string literal.
	///
	/// @param result The Result code to evaluate.
	///
	/// @return A null-terminated string describing the result.
	constexpr const char* GetResultString(const Result result) {
		switch (result) {
			case Result::Ok: return "Ok";

			case Result::Error: return "Error (generic)";
			case Result::NotInitialized: return "Engine is not initialized.";
			case Result::StateCorrupted: return "Engine state is corrupted.";
			case Result::AlreadyInitialized: return "Engine is already initialized.";
			case Result::InvalidArgs: return "Invalid arguments.";
			case Result::InvalidHandle: return "Invalid handle";
			case Result::ExpiredHandle: return "Expired handle";
			case Result::InvalidBus: return "Bus not found";
			case Result::InvalidRouting: return "Invalid routing";
			case Result::InvalidEffectSlot: return "Invalid effect slot";
			case Result::InvalidListener: return "Listener not found";

			case Result::CapacityReached: return "Pool capacity reached (generic)";
			case Result::SoundCapacityReached: return "Resident/Stream sound pool capacity reached";
			case Result::VoiceCapacityReached: return "Voice pool capacity reached";
			case Result::StreamCapacityReached: return "Stream pool capacity reached";
			case Result::BusCapacityReached: return "Bus pool capacity reached";
			case Result::EffectCapacityReached: return "Effect pool capacity reached";

			case Result::MixerCommandQueueFull: return "Mixer command queue full";
			case Result::MixerEventQueueFull: return "Mixer event queue full";
			case Result::IoStreamRequestQueueFull: return "I/O stream request queue full";
			case Result::IoLoadRequestQueueFull: return "I/O load request queue full";

			case Result::PlaybackCorrupted: return "Playback corrupted";

			case Result::SoundLoadError: return "Sound load error";
			case Result::FileReadError: return "File read error";
			case Result::UnsupportedFormat: return "Unsupported format";

			case Result::SystemError: return "System error";
			case Result::DeviceNotFound: return "Device not found";
			case Result::DeviceFailed: return "Device Failed";
			case Result::ClientFailed: return "Client Failed";

			default: return "No string is defined for this result type";
		}
	}
}