#pragma once

#include "backend/DeviceManager.h"

#include <atomic>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmdeviceapi.h>
#include <wrl/client.h>

namespace dalia {

	class WindowsDeviceManager : public DeviceManager {
	public:
		WindowsDeviceManager();
		~WindowsDeviceManager() override;

		Result Initialize() override;

		std::vector<OutputDeviceInfo> Enumerate() override;

		bool PollDefaultOutputDeviceChanged(std::string& newDeviceId) override;

		std::unique_ptr<OutputDevice> CreateDevice(const char* identifier, uint32_t engineSampleRate) override;
		std::unique_ptr<OutputDevice> CreateNullDevice(uint32_t engineSampleRate, uint32_t periodSizeInFrames) override;

	private:
		class NotificationClient final : public IMMNotificationClient {
		public:
			NotificationClient(std::atomic<bool>& changeFlag, std::string& idStr, std::mutex& mtx);
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
			std::string& m_idStr;
			std::mutex& m_mutex;

		};

		Microsoft::WRL::ComPtr<NotificationClient> m_notificationClient;
		Microsoft::WRL::ComPtr<IMMDeviceEnumerator> m_enumerator;

		std::mutex m_notificationMutex;
		std::string m_notificationDefaultId;
		std::atomic<bool> m_defaultOutputDeviceChangedFlag{false};

	};
}
