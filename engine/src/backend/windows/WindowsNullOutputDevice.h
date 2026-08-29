#pragma once

#include "backend/OutputDevice.h"

#include <atomic>
#include <thread>
#include <memory>
#include <cstdint>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace dalia {

	class RtSystem;

	class WindowsNullOutputDevice : public OutputDevice {
	public:
		WindowsNullOutputDevice(uint32_t engineSampleRate, uint32_t periodSizeInFrames);
		~WindowsNullOutputDevice() override;

		Result Start(RtSystem* system) override;
		void Stop() override;

		bool HasFailed() const override;

		const std::string& GetIdentifier() const override;
		const std::string& GetName() const override;
		uint32_t GetChannelCount() const override;
		SpeakerLayout GetSpeakerLayout() const override;

	private:
		void AudioThreadMain();

		RtSystem* m_system = nullptr;

		std::thread m_audioThread;
		std::atomic<bool> m_isRunning{false};

		std::string m_identifier = "No Output Device";
		std::string m_name = "null_device";
		uint32_t m_sampleRate = 0;
		uint32_t m_periodSizeInFrames = 0;

		std::unique_ptr<float[]> m_voidBuffer;

	};
}
