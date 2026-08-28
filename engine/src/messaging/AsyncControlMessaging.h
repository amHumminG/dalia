#pragma once

#include <cstdint>

#include "dalia/audio/DeviceControl.h"

namespace dalia {

	class OutputDevice;

	struct AsyncControlRequest {
		enum class Type : uint8_t {
			None,

			SwapOutputDevice,
		} type = Type::None;

		union Data {
			struct {
				char targetOutputDeviceId[MAX_DEVICE_STRING_LEN];
				uint32_t sampleRate;
			} swapOutputDevice;
		} data = {};

		static AsyncControlRequest SwapOutputDevice(const char* targetOutputDeviceId, uint32_t sampleRate) {
			AsyncControlRequest req;
			req.type = Type::SwapOutputDevice;
			snprintf(req.data.swapOutputDevice.targetOutputDeviceId, MAX_DEVICE_STRING_LEN, "%s", targetOutputDeviceId);
			req.data.swapOutputDevice.sampleRate = sampleRate;
			return req;
		}
	};

	struct AsyncControlEvent {
		enum class Type {
			None,

			OutputDeviceSwapped,
		} type = Type::None;

		union Data {
			struct {
				OutputDevice* newDevice;
				bool fellBackToDefault;
			} swapOutputDevice;
		} data = {};

		static AsyncControlEvent OutputDeviceSwapped(OutputDevice* newOutputDevice, bool fellBackToDefault) {
			AsyncControlEvent ev;
			ev.type = Type::OutputDeviceSwapped;
			ev.data.swapOutputDevice.newDevice = newOutputDevice;
			ev.data.swapOutputDevice.fellBackToDefault = fellBackToDefault;
			return ev;
		}
	};

}
