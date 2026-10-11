#pragma once

#include "backend/OutputDevice.h"
#include "mixer/Speakers.h"
#include "core/Constants.h"

#include <cstdint>
#include <string>
#include <thread>
#include <atomic>

namespace dalia {

	class MixerSystem;

	class NullOutputDevice : public OutputDevice {
	public:
		NullOutputDevice(uint32_t sampleRate, uint32_t periodSizeInFrames);
		~NullOutputDevice();

		Result Start(MixerSystem* mixerSystem) override;
		void Stop() override;

		void SetOutputFormat(uint32_t channels, SpeakerLayout layout);

		const std::string& GetIdentifier() const override;
		const std::string& GetName() const override;
		uint32_t GetChannelCount() const override;
		SpeakerLayout GetSpeakerLayout() const override;

	private:
		void ThreadMain();

		MixerSystem* m_mixerSystem = nullptr;

		std::thread m_thread;
		std::atomic<bool> m_isRunning{false};
		std::unique_ptr<float[]> m_voidBuffer;

		std::string m_identifier = "No Output Device";
		std::string m_name = "null_device";
		uint32_t m_sampleRate = 0;
		uint32_t m_channels = CHANNELS_MONO;
		uint32_t m_periodSizeInFrames = 0;
		SpeakerLayout m_speakerLayout = SpeakerLayout::Mono;
	};
}
