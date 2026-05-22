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
		ListenerNotFound			= -9,


		// Pool Capacity Errors
		PoolExhausted				= -100,
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
		SystemError					= -500,
		DeviceNotFound				= -501,
		DeviceFailed				= -502,
		ClientFailed				= -503,

	};

	constexpr const char* GetResultString(const Result result) {
		switch (result) {
			case Result::Ok: return "Ok";

			case Result::Error: return "Error (generic)";
			case Result::NotInitialized: return "Engine is not initialized.";
			case Result::AlreadyInitialized: return "Engine is already initialized.";
			case Result::InvalidHandle: return "Invalid handle";
			case Result::ExpiredHandle: return "Expired handle";
			case Result::BusNotFound: return "Bus not found";
			case Result::InvalidRouting: return "Invalid routing";
			case Result::InvalidEffectSlot: return "Invalid effect slot";
			case Result::ListenerNotFound: return "Listener not found";

			case Result::PoolExhausted: return "Pool exhausted (generic)";
			case Result::ResidentSoundPoolExhausted: return "Resident sound pool exhausted";
			case Result::StreamSoundPoolExhausted: return "Stream sound pool exhausted";
			case Result::VoicePoolExhausted: return "Voice pool exhausted";
			case Result::StreamPoolExhausted: return "Stream pool exhausted";
			case Result::BusPoolExhausted: return "Bus pool exhausted";
			case Result::BiquadFilterPoolExhausted: return "Biquad filter pool exhausted";

			case Result::RtCommandQueueFull: return "Real-time command queue full";
			case Result::RtEventQueueFull: return "Real-time event queue full";
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

			default: return "Error (unknown)";
		}
	}
}