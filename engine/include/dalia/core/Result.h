#pragma once

#define DALIA_OK(res) (static_cast<int>(res) >= 0)
#define DALIA_ERROR(res) (static_cast<int>(res) < 0)

namespace dalia {

	enum class Result : int {
		Ok = 0,

		// Generic Errors [-1, -99]
		Error = -1,
		NotInitialized = -2,
		AlreadyInitialized = -3,

		// Backed Errors [-100, -199]
		DeviceFailed = -100,
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