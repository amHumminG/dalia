#include "backend/PlatformThread.h"

#include "core/Logger.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace dalia {

	void PlatformThread::SetCurrentThreadPriority(ThreadPriority priority) {
		int winPriority = THREAD_PRIORITY_NORMAL;

		switch (priority) {
			case ThreadPriority::Normal: winPriority		= THREAD_PRIORITY_NORMAL; break;
			case ThreadPriority::High: winPriority			= THREAD_PRIORITY_HIGHEST; break;
			case ThreadPriority::TimeCritical: winPriority	= THREAD_PRIORITY_TIME_CRITICAL; break;
		}

		if (!SetThreadPriority(GetCurrentThread(), winPriority)) {
			DALIA_LOG_WARN(LOG_CTX_BACKEND, "Failed to set thread priority.");
		}
	}
}
