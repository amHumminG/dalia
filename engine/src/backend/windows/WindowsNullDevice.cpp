#include "backend/windows/WindowsNullDevice.h"

#include "mixer/RtSystem.h"
#include "core/Logger.h"

#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

namespace dalia {

	WindowsNullDevice::WindowsNullDevice(uint32_t engineSampleRate, uint32_t periodSizeInFrames)
		: m_sampleRate(engineSampleRate), m_periodSizeInFrames(periodSizeInFrames) {
		m_voidBuffer = std::make_unique<float[]>(m_periodSizeInFrames);
	}

	WindowsNullDevice::~WindowsNullDevice() {
		Stop();
	}

	Result WindowsNullDevice::Start(RtSystem* system) {
		if (m_isRunning.load(std::memory_order_relaxed)) return Result::Ok;

		m_system = system;
		m_isRunning.store(true, std::memory_order_release);

		m_audioThread = std::thread(&WindowsNullDevice::AudioThreadMain, this);
		return Result::Ok;
	}

	void WindowsNullDevice::Stop() {
		if (!m_isRunning.exchange(false, std::memory_order_release)) return;

		if (m_audioThread.joinable()) m_audioThread.join();
		m_system = nullptr;
	}

	uint32_t WindowsNullDevice::GetChannelCount() const {
		return 1;
	}

	SpeakerLayout WindowsNullDevice::GetSpeakerLayout() const {
		return SpeakerLayout::Mono;
	}

	void WindowsNullDevice::AudioThreadMain() {
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

		// Create high resolution timer
		HANDLE timer = CreateWaitableTimerExW(
			nullptr,
			nullptr,
			CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
			TIMER_ALL_ACCESS
		);

		if (!timer) {
			DALIA_LOG_WARN(LOG_CTX_BACKEND, "Failed to create high-resolution timer. Falling back to legacy timer.");
			timer = CreateWaitableTimerW(nullptr, FALSE, nullptr);
		}

		// Timer uses 100-nanosecond intervals. Negative values indicate relative time
		LONGLONG interval100ns = -static_cast<LONGLONG>((m_periodSizeInFrames * 10000000ULL) / m_sampleRate);

		LARGE_INTEGER dueTime;
		while (m_isRunning.load(std::memory_order_relaxed)) {
			dueTime.QuadPart = interval100ns;

			SetWaitableTimer(timer, &dueTime, 0, nullptr, nullptr, FALSE);
			WaitForSingleObject(timer, INFINITE);

			if (m_system) m_system->Tick(m_voidBuffer.get(), m_periodSizeInFrames);
		}

		if (timer) CloseHandle(timer);
	}
}
