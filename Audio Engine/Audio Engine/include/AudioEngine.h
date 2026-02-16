#pragma once
#include "Result.h"
#include <memory>

struct ma_device;

namespace placeholder_name {

	class AudioEngine {
	public:
		AudioEngine();
		~AudioEngine();

		Result init();
		Result deinit();

	private:
		static void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, uint32_t frameCount);

		bool m_initialized = false;
		std::unique_ptr<ma_device> m_device; // Miniaudio playback device

	};
}