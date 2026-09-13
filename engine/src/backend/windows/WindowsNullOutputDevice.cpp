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
		m_mixerSystem->SetOutputFormat(m_channels, m_speakerLayout);
		m_mixerSystem->Wake();

		m_isRunning.store(true, std::memory_order_release);

		m_thread = std::thread(&WindowsNullOutputDevice::ThreadMain, this);
		return Result::Ok;
	}

	void WindowsNullOutputDevice::Stop() {
		if (!m_isRunning.exchange(false, std::memory_order_release)) return;

		if (m_thread.joinable()) m_thread.join();
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
		return m_channels;
	}

	SpeakerLayout WindowsNullOutputDevice::GetSpeakerLayout() const {
		return m_speakerLayout;
	}

	void WindowsNullOutputDevice::ThreadMain() {
		PlatformThread::SetCurrentThreadPriority(ThreadPriority::TimeCritical);

		HighResTimer timer;
		uint32_t periodMicroseconds = (m_periodSizeInFrames * 1000000) / m_sampleRate;

		auto lastWakeTime = std::chrono::steady_clock::now();

		while (m_isRunning.load(std::memory_order_relaxed)) {
			timer.SleepMicroseconds(periodMicroseconds);
			if (!m_isRunning.load(std::memory_order_relaxed)) break;

			auto now = std::chrono::steady_clock::now();
			auto actualSleepUs = std::chrono::duration_cast<std::chrono::microseconds>(now - lastWakeTime).count();
			lastWakeTime = now;

			if (actualSleepUs < periodMicroseconds - 1000) { // If it woke up more than 1ms early
				DALIA_LOG_WARN(LOG_CTX_BACKEND, "TIMER BUG: Asked to sleep 10ms, but woke up in %lld microseconds!", actualSleepUs);
			}

			if (m_mixerSystem) {
				m_mixerSystem->DiscardAudio(m_periodSizeInFrames);
				m_mixerSystem->Wake();
			}
		}
	}
}
