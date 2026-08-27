#pragma once

#include "backend/AudioDevice.h"

#include <atomic>
#include <thread>
#include <memory>
#include <cstdint>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace dalia {

	class RtSystem;

	class WindowsNullDevice : public AudioDevice {
	public:
		WindowsNullDevice(uint32_t engineSampleRate, uint32_t periodSizeInFrames);
		~WindowsNullDevice() override;

		Result Start(RtSystem* system) override;
		void Stop() override;

		const std::string& GetName() const override;
		uint32_t GetChannelCount() const override;
		SpeakerLayout GetSpeakerLayout() const override;

	private:
		void AudioThreadMain();

		RtSystem* m_system = nullptr;

		std::thread m_audioThread;
		std::atomic<bool> m_isRunning{false};

		std::string m_name = "Null Device";
		uint32_t m_sampleRate = 0;
		uint32_t m_periodSizeInFrames = 0;

		std::unique_ptr<float[]> m_voidBuffer;

	};
}
