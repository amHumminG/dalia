#pragma once

#include "dalia/core/Result.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <avrt.h>

#include <atomic>
#include <thread>
#include <wrl/client.h>

namespace dalia {

	class RtSystem;

	class WasapiBackend {
	public:
		WasapiBackend(RtSystem* rtSystem);
		~WasapiBackend();

		Result Init();

		Result Start();

		void Stop();

		uint32_t GetSampleRate() const { return m_sampleRate; }
		uint32_t GetChannelCount() const { return m_channelCount; }
		uint32_t GetPeriodSizeInFrames() const { return m_periodSizeInFrames; }

	private:
		void AudioThreadMain();

		RtSystem* m_rtSystem = nullptr;

		Microsoft::WRL::ComPtr<IMMDevice> m_device;
		Microsoft::WRL::ComPtr<IAudioClient> m_audioClient;
		Microsoft::WRL::ComPtr<IAudioRenderClient> m_renderClient;

		HANDLE m_bufferEvent = nullptr;
		HANDLE m_shutdownEvent = nullptr;

		std::thread m_audioThread;
		std::atomic<bool> m_isRunning = false;

		uint32_t m_sampleRate = 0;
		uint32_t m_channelCount = 0;
		uint32_t m_periodSizeInFrames = 0;
	};
}

