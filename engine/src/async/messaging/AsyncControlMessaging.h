#pragma once

#include "core/SPSCRingBuffer.h"
#include "dalia/DeviceControl.h"

#include <cstdint>
#include <cstdio>

namespace dalia {

	class OutputDevice;

	struct AsyncControlCommand {
		enum class Type : uint8_t {
			None,

			SwapOutputDevice,


			Count // Determines the number of types
		} type = Type::None;

		union Data {
			struct {
				OutputDevice* oldDevice;
				char targetOutputDeviceId[MAX_STR_LEN_DEVICE];
				uint32_t sampleRate;
			} swapOutputDevice;
		} data = {};

		static AsyncControlCommand SwapOutputDevice(OutputDevice* oldDevice, const char* targetOutputDeviceId,
			uint32_t sampleRate) {
			AsyncControlCommand req;
			req.type = Type::SwapOutputDevice;
			req.data.swapOutputDevice.oldDevice = oldDevice;
			snprintf(req.data.swapOutputDevice.targetOutputDeviceId, MAX_STR_LEN_DEVICE, "%s", targetOutputDeviceId);
			req.data.swapOutputDevice.sampleRate = sampleRate;
			return req;
		}
	};

	struct AsyncControlEvent {
		enum class Type {
			None,

			OutputDeviceSwapped,


			Count // Determines the number of types
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

	// --- Queues ---

	using AsyncControlCommandQueue = SPSCRingBuffer<AsyncControlCommand>;
	using AsyncControlEventQueue = SPSCRingBuffer<AsyncControlEvent>;
}
