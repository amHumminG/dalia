#include "backend/windows/WindowsDeviceManager.h"

#include "backend/windows/WasapiOutputDevice.h"
#include "backend/windows/WindowsNullOutputDevice.h"
#include "core/Logger.h"

#include <functiondiscoverykeys_devpkey.h> // For PKEY_Device_FriendlyName
#include <cstring>

namespace dalia {

	// --- Notification Client Implementation ---

	WindowsDeviceManager::NotificationClient::NotificationClient(std::atomic<bool>& changeFlag, std::string& idStr, std::mutex& mtx)
		: m_changeFlag(changeFlag), m_idStr(idStr), m_mutex(mtx) {}

	ULONG STDMETHODCALLTYPE WindowsDeviceManager::NotificationClient::AddRef() {
		return InterlockedIncrement(&m_refCount);
	}

	ULONG STDMETHODCALLTYPE WindowsDeviceManager::NotificationClient::Release() {
		ULONG count = InterlockedDecrement(&m_refCount);
		if (count == 0) delete this;

		return count;
	}

	HRESULT STDMETHODCALLTYPE WindowsDeviceManager::NotificationClient::QueryInterface(REFIID riid, void** ppv) {
		if (riid == __uuidof(IUnknown) || riid == __uuidof(IMMNotificationClient)) {
			*ppv = static_cast<IMMNotificationClient*>(this);
			AddRef();
			return S_OK;
		}
		*ppv = nullptr;

		return E_NOINTERFACE;
	}

	HRESULT STDMETHODCALLTYPE WindowsDeviceManager::NotificationClient::OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR pwstrDeviceId) {
		// Only account for cases where the default rendering device changed
		if (flow == eRender && role == eConsole) {
			// Capture new default device id
			int size = WideCharToMultiByte(CP_UTF8, 0, pwstrDeviceId, -1, nullptr, 0, nullptr, nullptr);
			if (size > 0) {
				std::string newId(size - 1, 0);
				WideCharToMultiByte(CP_UTF8, 0, pwstrDeviceId, -1, newId.data(), size, nullptr, nullptr);

				// Lock mutex while copying
				std::lock_guard<std::mutex> lock(m_mutex);
				m_idStr = std::move(newId);
				m_changeFlag.store(true, std::memory_order_release);
			}
		}

		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE WindowsDeviceManager::NotificationClient::OnDeviceStateChanged(LPCWSTR /*pwstrDeviceId*/, DWORD /*dwNewState*/) {
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE WindowsDeviceManager::NotificationClient::OnDeviceAdded(LPCWSTR /*pwstrDeviceId*/) {
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE WindowsDeviceManager::NotificationClient::OnDeviceRemoved(LPCWSTR /*pwstrDeviceId*/) {
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE WindowsDeviceManager::NotificationClient::OnPropertyValueChanged(LPCWSTR /*pwstrDeviceId*/, const PROPERTYKEY /*key*/) {
		return S_OK; // Ignore volume/property changes
	}

	// -------

	WindowsDeviceManager::WindowsDeviceManager() = default;

	WindowsDeviceManager::~WindowsDeviceManager() {
		if (m_enumerator && m_notificationClient) {
			m_enumerator->UnregisterEndpointNotificationCallback(m_notificationClient.Get());
		}

		m_enumerator.Reset();
		m_notificationClient.Reset();

		CoUninitialize();
	}

	Result WindowsDeviceManager::Initialize() {
		// Initialize COM for this thread
		HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
		if (FAILED(hr)) return Result::SystemError;

		// Create device enumerator
		hr = CoCreateInstance(
			__uuidof(MMDeviceEnumerator),
			nullptr,
			CLSCTX_ALL,
			IID_PPV_ARGS(&m_enumerator)
		);
		if (FAILED(hr)) return Result::SystemError;

		// Create notification client
		m_notificationClient = new NotificationClient(m_defaultOutputDeviceChangedFlag, m_notificationDefaultId, m_notificationMutex);
		hr = m_enumerator->RegisterEndpointNotificationCallback(m_notificationClient.Get());
		if (FAILED(hr)) return Result::SystemError;

		return Result::Ok;
	}

	std::vector<OutputDeviceInfo> WindowsDeviceManager::Enumerate() {
		std::vector<OutputDeviceInfo> deviceList;
		if (!m_enumerator) return deviceList;

		// Find the current default device ID
		Microsoft::WRL::ComPtr<IMMDevice> defaultDevice;
		LPWSTR defaultIdW = nullptr;
		if (SUCCEEDED(m_enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &defaultDevice))) {
			defaultDevice->GetId(&defaultIdW);
		}

		Microsoft::WRL::ComPtr<IMMDeviceCollection> collection;
		if (FAILED(m_enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &collection))) {
			if (defaultIdW) CoTaskMemFree(defaultIdW);
			return deviceList;
		}

		UINT count = 0;
		collection->GetCount(&count);

		// Get all other devices
		for (UINT i = 0; i < count; i++) {
			Microsoft::WRL::ComPtr<IMMDevice> device;
			if (FAILED(collection->Item(i, &device))) continue;

			OutputDeviceInfo info{};

			// Get device id
			LPWSTR deviceIdW = nullptr;
			if (SUCCEEDED(device->GetId(&deviceIdW))) {
				WideCharToMultiByte(CP_UTF8, 0, deviceIdW, -1, info.identifier, MAX_DEVICE_STR_LEN, nullptr, nullptr);
				info.identifier[MAX_DEVICE_STR_LEN - 1] = '\0';

				info.isDefault = (defaultIdW && wcscmp(deviceIdW, defaultIdW) == 0);
				CoTaskMemFree(deviceIdW);
			}

			// Get friendly name
			Microsoft::WRL::ComPtr<IPropertyStore> props;
			if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, &props))) {
				PROPVARIANT varName;
				PropVariantInit(&varName);
				if (SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &varName)) && varName.pwszVal) {
					WideCharToMultiByte(CP_UTF8, 0, varName.pwszVal, -1, info.name, MAX_DEVICE_STR_LEN, nullptr, nullptr);
					info.name[MAX_DEVICE_STR_LEN - 1] = '\0';
				}
				PropVariantClear(&varName);
			}

			deviceList.push_back(info);
		}

		if (defaultIdW) CoTaskMemFree(defaultIdW);
		return deviceList;
	}

	bool WindowsDeviceManager::PollDefaultOutputDeviceChanged(std::string& newDeviceId) {
		if (m_defaultOutputDeviceChangedFlag.exchange(false, std::memory_order_acquire)) {
			// Lock mutex while we read the string
			std::lock_guard<std::mutex> lock(m_notificationMutex);
			newDeviceId = m_notificationDefaultId;
			return true;
		}

		return false;
	}

	std::unique_ptr<OutputDevice> WindowsDeviceManager::CreateDevice(const char* identifier, uint32_t engineSampleRate) {
		Microsoft::WRL::ComPtr<IMMDevice> device;
		HRESULT hr = S_OK;

		if (!identifier || std::strcmp(identifier, "default") == 0 || std::strlen(identifier) == 0) {
			hr = m_enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device); // Get OS default device
		}
		else {
			// Convert identifier string and get device
			int size = MultiByteToWideChar(CP_UTF8, 0, identifier, -1, nullptr, 0);
			if (size > 0) {
				std::wstring wIdentifier(size - 1, 0);
				MultiByteToWideChar(CP_UTF8, 0, identifier, -1, wIdentifier.data(), size);
				hr = m_enumerator->GetDevice(wIdentifier.c_str(), &device);
			}
		}

		if (FAILED(hr) || !device) return nullptr;

		auto wasapiDevice = std::make_unique<WasapiOutputDevice>(device);
		if (wasapiDevice->Initialize(engineSampleRate) != Result::Ok) return nullptr; // Negotiate format and buffer size

		return wasapiDevice;
	}

	std::unique_ptr<OutputDevice> WindowsDeviceManager::CreateNullDevice(uint32_t targetSampleRate, uint32_t periodSizeInFrames) {
		return std::make_unique<WindowsNullOutputDevice>(targetSampleRate, periodSizeInFrames);
	}
}


