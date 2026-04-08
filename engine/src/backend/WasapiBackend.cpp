#include "backend/WasapiBackend.h"

#include "core/Logger.h"
#include "mixer/RtSystem.h"

#include <corecrt_malloc.h>

#ifndef AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM
#define AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM 0x80000000
#endif

namespace dalia {

	WasapiBackend::WasapiBackend(RtSystem* rtSystem)
		: m_rtSystem(rtSystem) {
		m_bufferEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		m_shutdownEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
	}

	WasapiBackend::~WasapiBackend() {
		Stop();

		if (m_bufferEvent) CloseHandle(m_bufferEvent);
		if (m_shutdownEvent) CloseHandle(m_shutdownEvent);

		// Clean up the COM library
		CoUninitialize();
	}

	Result WasapiBackend::Init() {
		// Initialize COM
		HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
		if (FAILED(hr)) return Result::SystemError;

		// Create device enumerator
		Microsoft::WRL::ComPtr<IMMDeviceEnumerator> enumerator;
		hr = CoCreateInstance(
			__uuidof(MMDeviceEnumerator),
			nullptr,
			CLSCTX_ALL,
			IID_PPV_ARGS(&enumerator)
		);
		if (FAILED(hr)) return Result::SystemError;

		// Get the default playback device
		hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &m_device);
		if (FAILED(hr)) return Result::DeviceNotFound;

		// Activate the audio client interface for the playback device
		hr = m_device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&m_audioClient);
		if (FAILED(hr)) return Result::DeviceFailed;

		// Ask OS what the current output format is (sampleRate and channelCount)
		WAVEFORMATEX* mixFormat = nullptr;
		hr = m_audioClient->GetMixFormat(&mixFormat);
		if (FAILED(hr)) return Result::ClientFailed;

		m_sampleRate = mixFormat->nSamplesPerSec;
		m_channelCount = mixFormat->nChannels;

		// Initialize WASAPI in shared mode
		DWORD streamFlags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM;
		hr = m_audioClient->Initialize(
			AUDCLNT_SHAREMODE_SHARED,
			streamFlags,
			0,
			0,
			mixFormat,
			nullptr
		);

		// Free mixFormat pointer
		CoTaskMemFree(mixFormat);

		if (FAILED(hr)) return Result::ClientFailed;

		// Bind wake-up event
		hr = m_audioClient->SetEventHandle(m_bufferEvent);
		if (FAILED(hr)) return Result::ClientFailed;

		// Get the render interface
		hr = m_audioClient->GetService(IID_PPV_ARGS(&m_renderClient));
		if (FAILED(hr)) return Result::ClientFailed;

		UINT32 bufferSize = 0;
		hr = m_audioClient->GetBufferSize(&bufferSize);
		if (FAILED(hr)) return Result::ClientFailed;

		m_periodSizeInFrames = bufferSize;

		return Result::Ok;
	}

	Result WasapiBackend::Start() {
		if (m_isRunning.load(std::memory_order_relaxed)) return Result::Ok;

		// Tell OS to start the hardware stream
		HRESULT hr = m_audioClient->Start();
		if (FAILED(hr)) return Result::ClientFailed;

		m_isRunning.store(true, std::memory_order_release);
		m_audioThread = std::thread(&WasapiBackend::AudioThreadMain, this);

		return Result::Ok;
	}

	void WasapiBackend::Stop() {
		if (!m_isRunning.load(std::memory_order_relaxed)) return;

		m_isRunning.store(false, std::memory_order_release);
		SetEvent(m_shutdownEvent); // Wake up thread if sleeping

		if (m_audioThread.joinable()) m_audioThread.join();
		m_audioClient->Stop();
	}

	void WasapiBackend::AudioThreadMain() {
		// Make sure this thread is very high priority
		DWORD taskIndex = 0;
		HANDLE mmcssHandle = AvSetMmThreadCharacteristics(TEXT("Pro Audio"), &taskIndex);
		if (!mmcssHandle) {
			SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
			DALIA_LOG_WARN(LOG_CTX_CORE,
				"Failed to elevate audio thread priority level using MMCSS. Falling back to High Priority");
		}
		else {
			AvSetMmThreadPriority(mmcssHandle, AVRT_PRIORITY_CRITICAL); // Ensure priority level
		}

		HANDLE waitArray[2] = { m_shutdownEvent, m_bufferEvent };

		while (m_isRunning.load(std::memory_order_relaxed)) {
			// Sleep until OS pings one of the events
			DWORD waitResult = WaitForMultipleObjects(2, waitArray, FALSE, INFINITE);
			if (waitResult == WAIT_OBJECT_0) {
				// Shutdown event was signaled
				break;
			}
			else if (waitResult == WAIT_OBJECT_0 + 1) {
				// Callback event was signaled

				// Ask OS how many frames is already in the buffer
				UINT32 padding = 0;
				HRESULT hr = m_audioClient->GetCurrentPadding(&padding);
				if (FAILED(hr)) continue;

				UINT32 framesToWrite = m_periodSizeInFrames - padding;
				if (framesToWrite == 0) continue;

				// Ask OS for buffer pointer
				BYTE* pData = nullptr;
				hr = m_renderClient->GetBuffer(framesToWrite, &pData);
				if (FAILED(hr)) continue;

				// Let RtSystem fill up the buffer
				if (m_rtSystem) {
					float* outBuffer = reinterpret_cast<float*>(pData);
					m_rtSystem->Tick(outBuffer, framesToWrite);
				}
				else {
					// Write silence if RtSystem is not attached
					std::memset(pData, 0, framesToWrite * m_channelCount * sizeof(float));
				}

				m_renderClient->ReleaseBuffer(framesToWrite, 0);
			}
		}

		if (mmcssHandle) AvRevertMmThreadCharacteristics(mmcssHandle);
	}
}
