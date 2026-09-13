#include "backend/windows/WindowsNullOutputDevice.h"

#include "backend/PlatformThread.h"
#include "backend/HighResTimer.h"

#include "mixer/MixerSystem.h"
#include "core/Logger.h"

namespace dalia {

	WindowsNullOutputDevice::WindowsNullOutputDevice(uint32_t engineSampleRate, uint32_t periodSizeInFrames)
		: m_sampleRate(engineSampleRate), m_periodSizeInFrames(periodSizeInFrames) {
	}

	WindowsNullOutputDevice::~WindowsNullOutputDevice() {
		Stop();
	}

	Result WindowsNullOutputDevice::Start(MixerSystem* system) {
		if (m_isRunning.load(std::memory_order_relaxed)) return Result::Ok;

		m_mixerSystem = system;
		m_mixerSystem->SetOutputFormat(m_channelCount, m_speakerLayout);

		m_isRunning.store(true, std::memory_order_release);

		m_audioThread = std::thread(&WindowsNullOutputDevice::AudioThreadMain, this);
		return Result::Ok;
	}

	void WindowsNullOutputDevice::Stop() {
		if (!m_isRunning.exchange(false, std::memory_order_release)) return;

		if (m_audioThread.joinable()) m_audioThread.join();
		m_mixerSystem = nullptr;
	}

	bool WindowsNullOutputDevice::HasFailed() const {
		return false; // Cannot fail
	}

	const std::string& WindowsNullOutputDevice::GetIdentifier() const {
		return m_identifier;
	}

	const std::string& WindowsNullOutputDevice::GetName() const {
		return m_name;
	}

	uint32_t WindowsNullOutputDevice::GetChannelCount() const {
		return m_channelCount;
	}

	SpeakerLayout WindowsNullOutputDevice::GetSpeakerLayout() const {
		return m_speakerLayout;
	}

	void WindowsNullOutputDevice::AudioThreadMain() {
		PlatformThread::SetCurrentThreadPriority(ThreadPriority::TimeCritical);

		HighResTimer timer;
		uint32_t periodMicroseconds = (m_periodSizeInFrames * 1000000) / m_sampleRate;

		uint32_t samplesToDiscard = m_periodSizeInFrames * m_channelCount;
		while (m_isRunning.load(std::memory_order_relaxed)) {
			timer.SleepMicroseconds(periodMicroseconds);
			if (!m_isRunning.load(std::memory_order_relaxed)) break;

			if (m_mixerSystem) {
				m_mixerSystem->DiscardAudio(samplesToDiscard);
				m_mixerSystem->Wake();
			}
		}
	}
}
