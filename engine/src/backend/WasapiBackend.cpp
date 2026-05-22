#include "backend/WasapiBackend.h"

#include "core/Logger.h"
#include "mixer/RtSystem.h"

#include <algorithm>
#include <avrt.h>

#ifndef AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM
#define AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM 0x80000000
#endif

namespace dalia {

	WasapiBackend::WasapiBackend() {
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

		m_speakerLayout = SpeakerLayout::Stereo;
		if (mixFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
			WAVEFORMATEXTENSIBLE* extFormat = reinterpret_cast<WAVEFORMATEXTENSIBLE*>(mixFormat);
			DWORD mask = extFormat->dwChannelMask;

			// Windows standard masks
			switch (mask) {
				case KSAUDIO_SPEAKER_MONO:
					m_speakerLayout = SpeakerLayout::Mono;
					DALIA_LOG_DEBUG(LOG_CTX_BACKEND, "Detected speaker layout (Mono) with %u channel(s).", m_channelCount);
					break;
				case KSAUDIO_SPEAKER_STEREO:
					m_speakerLayout = SpeakerLayout::Stereo;
					DALIA_LOG_DEBUG(LOG_CTX_BACKEND, "Detected speaker layout (Stereo) with %u channel(s).", m_channelCount);
					break;
				case KSAUDIO_SPEAKER_5POINT1:
				case KSAUDIO_SPEAKER_5POINT1_SURROUND:
					m_speakerLayout = SpeakerLayout::Surround51;
					DALIA_LOG_DEBUG(LOG_CTX_BACKEND, "Detected speaker layout (5.1 Surround) with %u channel(s).", m_channelCount);
					break;
				case KSAUDIO_SPEAKER_7POINT1:
				case KSAUDIO_SPEAKER_7POINT1_SURROUND:
					m_speakerLayout = SpeakerLayout::Surround71;
					DALIA_LOG_DEBUG(LOG_CTX_BACKEND, "Detected speaker layout (7.1 Surround) with %u channel(s).", m_channelCount);
					break;
				default:
					DALIA_LOG_WARN(LOG_CTX_BACKEND, "Non-standard speaker layout detected (mask: 0x%X).", mask);
					// Attempt to guess the layout
					if (m_channelCount >= 8) m_speakerLayout = SpeakerLayout::Surround71;
					else if (m_channelCount >= 6) m_speakerLayout = SpeakerLayout::Surround51;
					else if (m_channelCount >= 2) m_speakerLayout = SpeakerLayout::Stereo;
					else m_speakerLayout = SpeakerLayout::Mono;
			}
		}
		else {
			DALIA_LOG_WARN(LOG_CTX_BACKEND, "Missing speaker layout. Falling back to estimation based on %u channel(s).", m_channelCount);
			if (m_channelCount >= 8) m_speakerLayout = SpeakerLayout::Surround71;
			else if (m_channelCount >= 6) m_speakerLayout = SpeakerLayout::Surround51;
			else if (m_channelCount >= 2) m_speakerLayout = SpeakerLayout::Stereo;
			else m_speakerLayout = SpeakerLayout::Mono;
		}

		// Initialize WASAPI in shared mode
		bool initializedLowLatency = false;

		// Attempt to initialize client with low latency period
		Microsoft::WRL::ComPtr<IAudioClient3> audioClient3;
		hr = m_audioClient->QueryInterface(IID_PPV_ARGS(&audioClient3));

		if (SUCCEEDED(hr)) {
			// Set client properties
			AudioClientProperties props = {0};
			props.cbSize = sizeof(AudioClientProperties);
			props.eCategory = AudioCategory_GameEffects; // Set games priority
			props.Options = AUDCLNT_STREAMOPTIONS_NONE;

			audioClient3->SetClientProperties(&props); // This can fail silently on older systems

			UINT32 defaultPeriod, fundamentalPeriod, minPeriod, maxPeriod;
			hr = audioClient3->GetSharedModeEnginePeriod(mixFormat, &defaultPeriod, &fundamentalPeriod,
				&minPeriod, &maxPeriod);

			if (SUCCEEDED(hr)) {
				// Snap to grid
				UINT32 targetPeriod = minPeriod;
				if (fundamentalPeriod > 0) targetPeriod = (targetPeriod / fundamentalPeriod) * fundamentalPeriod;
				targetPeriod = std::clamp(targetPeriod, minPeriod, maxPeriod);
				DALIA_LOG_DEBUG(LOG_CTX_BACKEND, "Target period: %d.", targetPeriod);

				// Attempt low-latency initialization
				hr = audioClient3->InitializeSharedAudioStream(
					AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
					targetPeriod,
					mixFormat,
					nullptr
				);

				// Handle locked scenario
				if (hr == AUDCLNT_E_ENGINE_PERIODICITY_LOCKED) {
					DALIA_LOG_WARN(LOG_CTX_BACKEND, "Engine periodicity is locked by another application.");

					hr = audioClient3->InitializeSharedAudioStream(
						AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
						defaultPeriod,
						mixFormat,
						nullptr
					);

					if (SUCCEEDED(hr)) {
						initializedLowLatency = true;
						m_periodSizeInFrames = defaultPeriod;
						DALIA_LOG_DEBUG(LOG_CTX_BACKEND, "Low-latency fallback to default period succeeded (%u frames).",
							defaultPeriod);
					}
				}
				else if (SUCCEEDED(hr)) {
					initializedLowLatency = true;
					m_periodSizeInFrames = targetPeriod;
					DALIA_LOG_DEBUG(LOG_CTX_BACKEND, "Low-latency initialization succeeded (%u frames).", targetPeriod);
				}
			}
		}

		if (!initializedLowLatency) {
			// Fallback if low latency negotiation failed (likely on an older Windows version)
			DALIA_LOG_WARN(LOG_CTX_BACKEND, "Low-latency initialization failed. Falling back to legacy audio client.");

			DWORD legacyFlags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM;
			hr = m_audioClient->Initialize(
				AUDCLNT_SHAREMODE_SHARED,
				legacyFlags,
				0,
				0,
				mixFormat,
				nullptr
			);

			if (FAILED(hr)) {
				DALIA_LOG_CRIT(LOG_CTX_BACKEND, "Failed to initialize audio client.");
				return Result::ClientFailed;
			}

			REFERENCE_TIME defaultDevicePeriod = 0;
			REFERENCE_TIME minDevicePeriod = 0;
			if (FAILED(m_audioClient->GetDevicePeriod(&defaultDevicePeriod, &minDevicePeriod))) {
				DALIA_LOG_CRIT(LOG_CTX_BACKEND, "Failed to query device period.");
				return Result::ClientFailed;
			}

			m_periodSizeInFrames = static_cast<UINT32>((defaultDevicePeriod * m_sampleRate) / 10000000);
			DALIA_LOG_DEBUG(LOG_CTX_BACKEND, "Legacy client initialization succeeded (%u frames).", m_periodSizeInFrames);
		}

		// Free mixFormat pointer
		CoTaskMemFree(mixFormat);

		if (FAILED(hr)) return Result::ClientFailed;

		// Bind wake-up event
		hr = m_audioClient->SetEventHandle(m_bufferEvent);
		if (FAILED(hr)) return Result::ClientFailed;

		// Get the render interface
		hr = m_audioClient->GetService(IID_PPV_ARGS(&m_renderClient));
		if (FAILED(hr)) return Result::ClientFailed;

		UINT32 bufferCapacity = 0;
		hr = m_audioClient->GetBufferSize(&bufferCapacity);
		if (FAILED(hr)) return Result::ClientFailed;

		m_bufferCapacityInFrames = bufferCapacity;
		DALIA_LOG_DEBUG(LOG_CTX_BACKEND, "Buffer capacity: %d.", bufferCapacity);

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

				UINT32 framesToWrite = m_bufferCapacityInFrames - padding;
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
