#pragma once

#include "backend/AudioDeviceManager.h"

#include <atomic>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmdeviceapi.h>
#include <wrl/client.h>

namespace dalia {

	class WindowsDeviceManager : public AudioDeviceManager {
	public:
		WindowsDeviceManager();
		~WindowsDeviceManager() override;

		Result Initialize() override;

		std::vector<AudioDeviceInfo> Enumerate() override;

		bool PollDeviceChanged() override;

		std::unique_ptr<AudioDevice> CreateDevice(const char* identifier, uint32_t engineSampleRate) override;
		std::unique_ptr<AudioDevice> CreateNullDevice(uint32_t engineSampleRate, uint32_t periodSizeInFrames) override;

	private:
		class NotificationClient final : public IMMNotificationClient {
		public:
			NotificationClient(std::atomic<bool>& changeFlag);
			~NotificationClient() = default;

			ULONG STDMETHODCALLTYPE AddRef() override;
			ULONG STDMETHODCALLTYPE Release() override;
			HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override;

			HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR pwstrDeviceId) override;
			HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR pwstrDeviceId, DWORD dwNewState) override;
			HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR pwstrDeviceId) override;
			HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR pwstrDeviceId) override;
			HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR pwstrDeviceId, const PROPERTYKEY key) override;

		private:
			LONG m_refCount = 1;
			std::atomic<bool>& m_changeFlag;
		};

		Microsoft::WRL::ComPtr<NotificationClient> m_notificationClient;
		Microsoft::WRL::ComPtr<IMMDeviceEnumerator> m_enumerator;

		std::atomic<bool> m_deviceChangedFlag{false};

	};
}
