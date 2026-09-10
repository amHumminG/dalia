#pragma once

namespace dalia {

	enum class ThreadPriority {
		Normal,
		High,
		TimeCritical
	};

	namespace PlatformThread {

		void SetCurrentThreadPriority(ThreadPriority priority);
	}
}