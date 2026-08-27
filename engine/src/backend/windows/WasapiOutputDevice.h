#pragma once

#include "backend/OutputDevice.h"
#include "mixer/Speakers.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <wrl/client.h>

#include <atomic>
#include <thread>

namespace dalia {

	class RtSystem;

	class WasapiOutputDevice : public OutputDevice {
	public:
		WasapiOutputDevice(Microsoft::WRL::ComPtr<IMMDevice> device);
		~WasapiOutputDevice() override;

		Result Initialize(uint32_t engineSampleRate);

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

		Microsoft::WRL::ComPtr<IMMDevice> m_device;
		Microsoft::WRL::ComPtr<IAudioClient> m_audioClient;
		Microsoft::WRL::ComPtr<IAudioRenderClient> m_renderClient;

		HANDLE m_bufferEvent = nullptr;
		HANDLE m_shutdownEvent = nullptr;

		std::thread m_audioThread;
		std::atomic<bool> m_isRunning{false};
		std::atomic<bool> m_hasFailed{false};

		std::string m_identifier;
		std::string m_name;
		uint32_t m_sampleRate = 0;
		uint32_t m_channelCount = 0;
		uint32_t m_periodSizeInFrames = 0;
		uint32_t m_bufferCapacityInFrames = 0;
		SpeakerLayout m_speakerLayout = SpeakerLayout::Mono;

	};
}
