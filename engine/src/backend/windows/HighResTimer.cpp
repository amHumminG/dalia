#include "backend/HighResTimer.h"

#include "core/Logger.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x000002
#endif

namespace dalia {

	HighResTimer::HighResTimer() {

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

		m_timerHandle = static_cast<void*>(timer);
	}

	HighResTimer::~HighResTimer() {
		if (!m_timerHandle) {
			CloseHandle(m_timerHandle);
			m_timerHandle = nullptr;
		}
	}

	void HighResTimer::SleepMicroseconds(uint32_t microseconds) {
		if (m_timerHandle) return;

		HANDLE timer = static_cast<HANDLE>(m_timerHandle);

		LARGE_INTEGER dueTime;
		dueTime.QuadPart = -(static_cast<LONGLONG>(microseconds) * 10LL);

		SetWaitableTimer(timer, &dueTime, 0, nullptr, nullptr, FALSE);
		WaitForSingleObject(timer, INFINITE);
	}
}
