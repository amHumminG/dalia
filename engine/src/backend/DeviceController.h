#pragma once

#include "backend/AudioBackend.h"
#include "backend/Windows/WasapiAudioBackend.h"
#include "backend/OutputDevice.h"
#include "backend/NullOutputDevice.h"
#include "core/Logger.h"
#include "core/FixedQueue.h"

#include <memory>



namespace dalia {

	struct AsyncControlCommand;
	struct AsyncControlEvent;
	class AsyncMessageBus;
	class AsyncMixerSystem;

	class DeviceController {
	public:
		DeviceController() = default;
		~DeviceController();

		Result Initialize(
			AsyncMessageBus* asyncMessageBus,
			AsyncMixerSystem* mixerSystem,
			uint32_t outSampleRate,
			uint32_t periodSizeInFrames,
			const std::string& outDeviceId = "default"
		);
		void Shutdown();

		void Update();

		uint32_t GetOutputDeviceCount();
		Result GetOutputDeviceInfo(uint32_t index, OutputDeviceInfo& info);
		Result GetActiveOutputDeviceInfo(OutputDeviceInfo& info);
		Result GetTargetOutputDeviceInfo(char* identifier, size_t maxLength);
		Result SetOutputDevice(const char* identifier);

		Result StartActiveOutputDevice(AsyncMixerSystem* mixerSystem);

		[[nodiscard]] SpeakerLayout GetActiveOutputSpeakerLayout() const;
		[[nodiscard]] uint32_t GetActiveOutputChannels() const;

		[[nodiscard]] uint32_t GetOutputSampleRate() const;
		[[nodiscard]] uint32_t GetPeriodSizeFrames() const;
		[[nodiscard]] uint32_t GetMaxPeriodSizeSamples() const;

	private:
		void SetupOutputDeviceDefault();
		void SetupOutputDeviceTargeted(const std::string& outputDeviceId);
		void SetupNullOutputDevice();

		void CheckForOutputDeviceChanges();
		void FinalizeOutputDeviceSwap(const AsyncControlEvent& ev);
		void ExecuteOutputDeviceSwap(const AsyncControlCommand& cmd); // Runs on async control thread

		AsyncMessageBus* m_asyncMessageBus = nullptr;
		AsyncMixerSystem* m_mixerSystem = nullptr;

		std::unique_ptr<AudioBackend> m_audioBackend;
		std::unique_ptr<OutputDevice> m_outputDevice;
		std::unique_ptr<NullOutputDevice> m_nullOutputDevice;
		OutputDevice* m_activeOutputDevice = nullptr; // Points to either outputDevice or nullDevice

		// Output device swapping
		std::vector<OutputDeviceInfo> m_cachedOutputDeviceList;
		std::string m_targetOutputDeviceId = "default";
		bool m_activeOutputDeviceIsDefault = true;
		bool m_pendingOutputDeviceSwap = false;
		bool m_isSwappingOutputDevice = false;

		// Output info
		SpeakerLayout m_speakerLayout = SpeakerLayout::Mono;
		uint32_t m_periodSizeFrames = 0;
		uint32_t m_outChannels = 0;
		uint32_t m_outSampleRate = 0;
	};
}
