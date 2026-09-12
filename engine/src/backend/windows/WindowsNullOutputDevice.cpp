#include "backend/windows/WindowsNullOutputDevice.h"

#include "backend/PlatformThread.h"
#include "backend/HighResTimer.h"

#include "mixer/MixerSystem.h"
#include "core/Logger.h"

namespace dalia {

	WindowsNullOutputDevice::WindowsNullOutputDevice(uint32_t engineSampleRate, uint32_t periodSizeInFrames)
		: m_sampleRate(engineSampleRate), m_periodSizeInFrames(periodSizeInFrames) {
		m_voidBuffer = std::make_unique<float[]>(m_periodSizeInFrames);
	}

	WindowsNullOutputDevice::~WindowsNullOutputDevice() {
		Stop();
	}

	Result WindowsNullOutputDevice::Start(MixerSystem* system) {
		if (m_isRunning.load(std::memory_order_relaxed)) return Result::Ok;

		m_system = system;
		m_isRunning.store(true, std::memory_order_release);

		m_audioThread = std::thread(&WindowsNullOutputDevice::AudioThreadMain, this);
		return Result::Ok;
	}

	void WindowsNullOutputDevice::Stop() {
		if (!m_isRunning.exchange(false, std::memory_order_release)) return;

		if (m_audioThread.joinable()) m_audioThread.join();
		m_system = nullptr;
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
		return 1;
	}

	SpeakerLayout WindowsNullOutputDevice::GetSpeakerLayout() const {
		return SpeakerLayout::Mono;
	}

	void WindowsNullOutputDevice::AudioThreadMain() {
		PlatformThread::SetCurrentThreadPriority(ThreadPriority::TimeCritical);

		HighResTimer timer;
		uint32_t periodMicroseconds = (m_periodSizeInFrames * 1000000) / m_sampleRate;

		while (m_isRunning.load(std::memory_order_relaxed)) {
			timer.SleepMicroseconds(periodMicroseconds);

			if (!m_isRunning.load(std::memory_order_relaxed)) break;
			if (m_system) m_system->Tick(m_voidBuffer.get(), m_periodSizeInFrames);
		}
	}
}
