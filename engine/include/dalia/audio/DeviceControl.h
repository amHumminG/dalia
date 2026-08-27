#pragma once

#include <cstdint>

namespace dalia {

	constexpr uint32_t MAX_DEVICE_STRING_LEN = 256;

	struct AudioDeviceInfo {
		char name[MAX_DEVICE_STRING_LEN];
		char identifier[MAX_DEVICE_STRING_LEN];
		bool isDefault = false;
	};
}
