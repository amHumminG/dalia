#pragma once

/// @file DeviceControl.h
/// @brief Audio output device metadata structures.

#include <cstdint>

namespace dalia {

	/// @brief The maximum string length (including null-terminator) for device names and identifiers.
	constexpr size_t MAX_STR_LEN_DEVICE = 256;

	/// @brief Contains metadata for an audio output device.
	struct OutputDeviceInfo {
		char name[MAX_STR_LEN_DEVICE]; // Readable device name.
		char identifier[MAX_STR_LEN_DEVICE]; // OS-level identifier.
		bool isDefault = false;	// True if the device is set as OS default.
	};
}
