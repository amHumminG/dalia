#pragma once

#include "backend/IAudioBackend.h"

#include "dalia/core/Result.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>

#include <atomic>
#include <thread>
#include <wrl/client.h>

namespace dalia {

	class RtSystem;

	class WasapiBackend : public IAudioBackend {
	public:
		WasapiBackend();
		~WasapiBackend();

		Result Init() override;
		Result Start() override;
		void Stop() override;

		void AttachSystem(RtSystem* system) override { m_rtSystem = system; }

		uint32_t GetSampleRate() const override { return m_sampleRate; }
		uint32_t GetChannelCount() const override { return m_channelCount; }
		uint32_t GetPeriodSizeInFrames() const override { return m_periodSizeInFrames; }
		uint32_t GetBufferCapacityInFrames() const override { return m_bufferCapacityInFrames; }

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
		uint32_t m_bufferCapacityInFrames = 0;
	};
}
