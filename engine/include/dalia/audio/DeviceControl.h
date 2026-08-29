#pragma once

#include <cstdint>

namespace dalia {

	constexpr size_t MAX_DEVICE_STR_LEN = 256; // The maximum string length (including null-terminator) for device names and identifiers.

	/// @brief Contains metadata for an audio output device.
	struct OutputDeviceInfo {
		char name[MAX_DEVICE_STR_LEN]; // Readable device name.
		char identifier[MAX_DEVICE_STR_LEN]; // OS-level identifier.
		bool isDefault = false;	// True if the device is set as OS default.
	};
}
