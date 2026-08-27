#include "backend/windows/WasapiOutputDevice.h"

#include "mixer/RtSystem.h"
#include "core/Logger.h"

#include <cstring>
#include <algorithm>
#include <functiondiscoverykeys_devpkey.h> // For PKEY_Device_FriendlyName
#include "avrt.h"

#ifndef AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM
#define AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM 0x80000000
#endif

namespace dalia {

	WasapiOutputDevice::WasapiOutputDevice(Microsoft::WRL::ComPtr<IMMDevice> device)
		: m_device(std::move(device)) {
		m_bufferEvent = CreateEvent(nullptr, FALSE, TRUE, nullptr);
		m_shutdownEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
	}

	WasapiOutputDevice::~WasapiOutputDevice() {
		Stop();
		if (m_bufferEvent) CloseHandle(m_bufferEvent);
		if (m_shutdownEvent) CloseHandle(m_shutdownEvent);
	}

	Result WasapiOutputDevice::Initialize(uint32_t engineSampleRate) {
		if (!m_device) return Result::DeviceFailed;

		// Extract friendly name
		Microsoft::WRL::ComPtr<IPropertyStore> props;
		if (SUCCEEDED(m_device->OpenPropertyStore(STGM_READ, &props))) {
			PROPVARIANT varName;
			PropVariantInit(&varName);
			if (SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &varName)) && varName.pwszVal) {
				int nameSize = WideCharToMultiByte(CP_UTF8, 0, varName.pwszVal, -1, nullptr, 0, nullptr, nullptr);
				if (nameSize > 0) {
					m_name.resize(nameSize - 1);
					WideCharToMultiByte(CP_UTF8, 0, varName.pwszVal, -1, m_name.data(), nameSize, nullptr, nullptr);
				}
			}
			PropVariantClear(&varName);
		}
		if (m_name.empty()) m_name = "Unknown WASAPI Device";

		HRESULT hr = m_device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&m_audioClient);
		if (FAILED(hr)) return Result::DeviceFailed;

		WAVEFORMATEX* mixFormat = nullptr;
		hr = m_audioClient->GetMixFormat(&mixFormat);
		if (FAILED(hr)) return Result::ClientFailed;

		mixFormat->nSamplesPerSec = engineSampleRate; // Set device sample rate to match engine output
		mixFormat->nAvgBytesPerSec = mixFormat->nSamplesPerSec * mixFormat->nBlockAlign; // Recalculate byte rate
		m_sampleRate = engineSampleRate;

		m_channelCount = mixFormat->nChannels;

		// --- Determine Speaker Layout ---
		m_speakerLayout = SpeakerLayout::Stereo;
		if (mixFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
			auto* extFormat = reinterpret_cast<WAVEFORMATEXTENSIBLE*>(mixFormat);
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
					DALIA_LOG_WARN(LOG_CTX_BACKEND,
						"Non-standard speaker layout detected (mask: 0x%X). Falling back to estimation based on %u channel(s).",
						mask, m_channelCount);
					if (m_channelCount >= 8) m_speakerLayout = SpeakerLayout::Surround71;
					else if (m_channelCount >= 6) m_speakerLayout = SpeakerLayout::Surround51;
					else if (m_channelCount >= 2) m_speakerLayout = SpeakerLayout::Stereo;
					else m_speakerLayout = SpeakerLayout::Mono;
			}
		}
		else {
			DALIA_LOG_WARN(LOG_CTX_BACKEND,
				"Missing speaker layout. Falling back to estimation based on %u channel(s).", m_channelCount);
			if (m_channelCount >= 8) m_speakerLayout = SpeakerLayout::Surround71;
			else if (m_channelCount >= 6) m_speakerLayout = SpeakerLayout::Surround51;
			else if (m_channelCount >= 2) m_speakerLayout = SpeakerLayout::Stereo;
			else m_speakerLayout = SpeakerLayout::Mono;
		}

		// --- Audio Client Initialization ---

		// Initialize WASAPI with auto-convert pcm for best device compatibility
		DWORD streamFlags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM;

		hr = m_audioClient->Initialize(
			AUDCLNT_SHAREMODE_SHARED,
			streamFlags,
			0,
			0,
			mixFormat,
			nullptr
		);

		if (FAILED(hr)) {
			DALIA_LOG_ERR(LOG_CTX_BACKEND, "Failed to initialize audio client.");
			return Result::ClientFailed;
		}

		REFERENCE_TIME defaultDevicePeriod = 0;
		REFERENCE_TIME minDevicePeriod = 0;
		if (FAILED(m_audioClient->GetDevicePeriod(&defaultDevicePeriod, &minDevicePeriod))) {
			DALIA_LOG_ERR(LOG_CTX_BACKEND, "Failed to query device period.");
			return Result::ClientFailed;
		}

		m_periodSizeInFrames = static_cast<UINT32>((defaultDevicePeriod * m_sampleRate) / 10000000);
		DALIA_LOG_DEBUG(LOG_CTX_BACKEND, "Audio client initialization succeeded with default periodicity (%u frames).",
			m_periodSizeInFrames);

		CoTaskMemFree(mixFormat); // Free mixFormat pointer

		// Bind wake-up event
		hr = m_audioClient->SetEventHandle(m_bufferEvent);
		if (FAILED(hr)) return Result::ClientFailed;

		// Get the render interface
		hr = m_audioClient->GetService(IID_PPV_ARGS(&m_renderClient));
		if (FAILED(hr)) return Result::ClientFailed;

		// Query final buffer capabilities
		UINT32 bufferCapacity = 0;
		hr = m_audioClient->GetBufferSize(&bufferCapacity);
		if (FAILED(hr)) return Result::ClientFailed;
		m_bufferCapacityInFrames = bufferCapacity;

		return Result::Ok;
	}

	Result WasapiOutputDevice::Start(RtSystem* system) {
		if (m_isRunning.load(std::memory_order_relaxed)) return Result::Ok;

		m_system = system;

		HRESULT hr = m_audioClient->Start();
		if (FAILED(hr)) return Result::ClientFailed;

		m_isRunning.store(true, std::memory_order_release);
		m_audioThread = std::thread(&WasapiOutputDevice::AudioThreadMain, this);

		return Result::Ok;
	}

	void WasapiOutputDevice::Stop() {
		if (!m_isRunning.exchange(false, std::memory_order_release)) return;

		SetEvent(m_shutdownEvent); // Wake up thread if it's asleep
		if (m_audioThread.joinable()) m_audioThread.join();

		if (m_audioClient) m_audioClient->Stop();
		m_system = nullptr;
	}

	bool WasapiOutputDevice::HasFailed() const {
		return m_hasFailed.load(std::memory_order_relaxed);
	}

	const std::string& WasapiOutputDevice::GetName() const {
		return m_name;
	}

	uint32_t WasapiOutputDevice::GetChannelCount() const {
		return m_channelCount;
	}

	SpeakerLayout WasapiOutputDevice::GetSpeakerLayout() const {
		return m_speakerLayout;
	}

	void WasapiOutputDevice::AudioThreadMain() {
		// Ensure this thread is very high priority
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

		HANDLE waitArray[2] = { m_shutdownEvent, m_bufferEvent }; // The events that the thread is waiting for

		while (m_isRunning.load(std::memory_order_relaxed)) {
			// Sleep until OS signals one of the events
			DWORD waitResult = WaitForMultipleObjects(2, waitArray, FALSE, INFINITE);
			if (waitResult == WAIT_OBJECT_0) {
				// Shutdown event was signaled
				break;
			}
			else if (waitResult == WAIT_OBJECT_0 + 1) {
				// Callback event was signaled

				// Get the number of frames already in the buffer
				UINT32 padding = 0;
				HRESULT hr = m_audioClient->GetCurrentPadding(&padding);
				if (hr == AUDCLNT_E_DEVICE_INVALIDATED || hr == AUDCLNT_E_RESOURCES_INVALIDATED) {
					DALIA_LOG_WARN(LOG_CTX_BACKEND,
						"Audio device invalidated during padding query. Stopping audio thread.");
					m_hasFailed.store(true, std::memory_order_release);
					break;
				}
				if (FAILED(hr)) continue;

				UINT32 framesToWrite = m_bufferCapacityInFrames - padding;
				if (framesToWrite == 0) continue;

				// Get the buffer pointer
				BYTE* pData = nullptr;
				hr = m_renderClient->GetBuffer(framesToWrite, &pData);
				if (hr == AUDCLNT_E_DEVICE_INVALIDATED || hr == AUDCLNT_E_RESOURCES_INVALIDATED) {
					DALIA_LOG_WARN(LOG_CTX_BACKEND,
						"Audio device invalidated during buffer fetch. Stopping audio thread.");
					m_hasFailed.store(true, std::memory_order_release);
					break;
				}
				if (FAILED(hr)) continue;

				// Let system fill the buffer (render the audio frame)
				if (m_system) {
					float* outBuffer = reinterpret_cast<float*>(pData);
					m_system->Tick(outBuffer, framesToWrite);
				}
				else {
					std::memset(pData, 0, framesToWrite * m_channelCount * sizeof(float));
				}

				hr = m_renderClient->ReleaseBuffer(framesToWrite, 0);
				if (hr == AUDCLNT_E_DEVICE_INVALIDATED || hr == AUDCLNT_E_RESOURCES_INVALIDATED) {
					DALIA_LOG_WARN(LOG_CTX_BACKEND, "Audio device invalidated during buffer release. Stopping audio thread.");
					m_hasFailed.store(true, std::memory_order_release);
					break;
				}
			}
		}

		if (mmcssHandle) AvRevertMmThreadCharacteristics(mmcssHandle);
	}
}
