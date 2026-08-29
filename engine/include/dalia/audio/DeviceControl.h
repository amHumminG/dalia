#pragma once

#include <cstdint>

namespace dalia {

	constexpr size_t MAX_DEVICE_STR_LEN = 256;

	/// @brief
	struct OutputDeviceInfo {
		char name[MAX_DEVICE_STR_LEN];
		char identifier[MAX_DEVICE_STR_LEN];
		bool isDefault = false;
	};
}
