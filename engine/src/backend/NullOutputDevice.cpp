#include "backend/NullOutputDevice.h"

#include "backend/PlatformThread.h"
#include "backend/HighResTimer.h"
#include "mixer/MixerSystem.h"

namespace dalia {

	NullOutputDevice::NullOutputDevice(uint32_t sampleRate, uint32_t periodSizeInFrames)
		: m_sampleRate(sampleRate), m_periodSizeInFrames(periodSizeInFrames) {}

	NullOutputDevice::~NullOutputDevice() {
		Stop();
	}

	Result NullOutputDevice::Start(MixerSystem* mixerSystem) {
		if (m_isRunning.load(std::memory_order_relaxed)) return Result::Ok;

		m_mixerSystem = mixerSystem;
		m_mixerSystem->SetOutputFormat(m_channels, m_speakerLayout);
		m_mixerSystem->Wake();

		m_isRunning.store(true, std::memory_order_release);
		m_thread = std::thread(&NullOutputDevice::ThreadMain, this);

		return Result::Ok;
	}

	void NullOutputDevice::Stop() {
		if (!m_isRunning.exchange(false, std::memory_order_release)) return;

		if (m_thread.joinable()) m_thread.join();
		m_mixerSystem = nullptr;
	}

	void NullOutputDevice::SetOutputFormat(uint32_t channels, SpeakerLayout layout) {
		m_channels = channels;
		m_speakerLayout = layout;
	}

	const std::string& NullOutputDevice::GetIdentifier() const {
		return m_identifier;
	}

	const std::string& NullOutputDevice::GetName() const {
		return m_name;
	}

	uint32_t NullOutputDevice::GetChannelCount() const {
		return m_channels;
	}

	SpeakerLayout NullOutputDevice::GetSpeakerLayout() const {
		return m_speakerLayout;
	}

	void NullOutputDevice::ThreadMain() {
		PlatformThread::SetCurrentThreadPriority(ThreadPriority::TimeCritical);

		HighResTimer timer;
		uint32_t periodMicroseconds = (m_periodSizeInFrames * 1000000) / m_sampleRate;

		while (m_isRunning.load(std::memory_order_relaxed)) {
			timer.SleepMicroseconds(periodMicroseconds);
			if (!m_isRunning.load(std::memory_order_relaxed)) break;

			if (m_mixerSystem) {
				m_mixerSystem->DiscardAudio(m_periodSizeInFrames);
				m_mixerSystem->Wake();
			}
		}
	}
}
