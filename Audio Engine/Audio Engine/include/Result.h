#pragma once

namespace placeholder_name {

	enum class Result : int {
		Ok = 0,
		Error = -1,

		// MiniAudio
		DeviceFailed = -3,
	};

	inline const char* GetErrorString(Result result) {
		switch (result) {
			case Result::Ok: return "Ok: No error.";
			case Result::Error: return "Generic error";
			case Result::DeviceFailed: return "Playback device failed";
			default: return "Unknown error";
			// Keep updating this
		}
	}
}