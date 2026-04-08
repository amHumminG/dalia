#pragma once

namespace dalia {

	enum class Result : int {
		Ok					= 0,

		// Generic Errors [-1, -99]
		Error						= -1,
		NotInitialized				= -2,
		AlreadyInitialized			= -3,
		InvalidHandle				= -4,
		ExpiredHandle				= -5,
		BusNotFound					= -6,
		InvalidRouting				= -7,
		InvalidEffectSlot			= -8,
		EffectAlreadyAttached		= -9,

		// Pool Capacity Errors
		// PoolExhausted				= -100,
		ResidentSoundPoolExhausted	= -101,
		StreamSoundPoolExhausted	= -102,
		VoicePoolExhausted			= -103,
		StreamPoolExhausted			= -104,
		BusPoolExhausted			= -105,
		BiquadFilterPoolExhausted	= -106,

		// Messaging Errors
		RtCommandQueueFull			= -200,
		RtEventQueueFull			= -201,
		IoStreamRequestQueueFull	= -202,
		IoLoadRequestQueueFull		= -203,

		// Playback Errors
		PlaybackCorrupted			= -303,

		// I/O Errors
		SoundLoadError				= -400,
		FileReadError				= -401,
		UnsupportedFormat			= -402,

		// Backed Errors
		DeviceFailed				= -500,
	};

	constexpr const char* GetErrorString(const Result result) {
		// Update this every time you add a new result
		switch (result) {
			case Result::Ok: return "Ok: No error.";

			case Result::Error: return "Generic error.";
			case Result::NotInitialized: return "Engine is not initialized.";
			case Result::AlreadyInitialized: return "Engine is already initialized.";

			case Result::DeviceFailed: return "Playback device failed.";
			default: return "Unknown error.";
		}
	}
}