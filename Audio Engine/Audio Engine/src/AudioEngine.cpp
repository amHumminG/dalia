#include "AudioEngine.h"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio/miniaudio.h"

namespace placeholder_name {

	AudioEngine::AudioEngine() {
		m_device = std::make_unique<ma_device>();
	}

	AudioEngine::~AudioEngine() {

	}

	Result AudioEngine::init() {
		if (m_initialized) return Result::Ok;

		ma_device_config config = ma_device_config_init(ma_device_type_playback);
		config.playback.format = ma_format_f32;
		config.playback.channels = 2; // Stereo playback only
		config.sampleRate = 44100; // Hz
		config.dataCallback = data_callback;
		config.pUserData = this;

		if (ma_device_init(NULL, &config, m_device.get()) != MA_SUCCESS) {
			// Log error here
			return Result::DeviceFailed;
		}

		if (ma_device_start(m_device.get()) != MA_SUCCESS) {
			// Log error here
			return Result::DeviceFailed;
		}

		// Could log initialization here
		m_initialized = true;
		return Result::Ok;
	}

	Result AudioEngine::deinit() {
		if (!m_initialized) return Result::Ok;

		if (ma_device_stop(m_device.get()) != MA_SUCCESS) {
			// Log error here
			return Result::DeviceFailed;
		}

		ma_device_uninit(m_device.get());
		m_initialized = false;
		// Could log deinitialization here
		return Result::Ok;
	}

	void AudioEngine::data_callback(ma_device* pDevice, void* pOutput, const void* pInput, uint32_t frameCount) {

	}

}

