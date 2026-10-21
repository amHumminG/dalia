#include "backend/PlatformThread.h"

#include "core/Logger.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "engine/DeviceCoordinator.h"

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

	PlatformThread::ScopedThreadRuntime::ScopedThreadRuntime() {
		HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
		// RPC_E_CHANGED_MODE means COM was already initialized on this thread but in a different mode (ignore it)
		if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
			DALIA_LOG_ERR(LOG_CTX_BACKEND, "Failed to initialize COM on background thread.");
		}
	}

	PlatformThread::ScopedThreadRuntime::~ScopedThreadRuntime() {
		CoUninitialize();
	}
}
