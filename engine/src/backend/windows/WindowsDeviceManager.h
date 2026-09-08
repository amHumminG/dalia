#pragma once

#include "backend/DeviceManager.h"
#include "core/SPSCRingBuffer.h"
#include "core/Constants.h"

#include <mmdeviceapi.h>
#include <wrl/client.h>

namespace dalia {

	class WindowsDeviceManager : public DeviceManager {
	public:
		WindowsDeviceManager();
		~WindowsDeviceManager() override;

		Result Initialize() override;

		std::vector<OutputDeviceInfo> Enumerate() override;

		bool PopDeviceChangeNotification(std::string& newDeviceId) override;

		std::unique_ptr<OutputDevice> CreateDevice(const char* identifier, uint32_t engineSampleRate) override;
		std::unique_ptr<OutputDevice> CreateNullDevice(uint32_t engineSampleRate, uint32_t periodSizeInFrames) override;

	private:
		class NotificationClient final : public IMMNotificationClient {
		public:
			NotificationClient(SPSCRingBuffer<OSDeviceNotification>& queue);
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
			SPSCRingBuffer<OSDeviceNotification>& m_notificationQueue;
		};

		Microsoft::WRL::ComPtr<NotificationClient> m_notificationClient;
		Microsoft::WRL::ComPtr<IMMDeviceEnumerator> m_enumerator;

		SPSCRingBuffer<OSDeviceNotification> m_notificationQueue{DEVICE_NOTIFICATION_QUEUE_CAPACITY};
	};
}
