#pragma once

namespace dalia {

	enum class ThreadPriority {
		Normal,
		High,
		TimeCritical
	};

	namespace PlatformThread {

		void SetCurrentThreadPriority(ThreadPriority priority);

		struct ScopedThreadRuntime {
			ScopedThreadRuntime();
			~ScopedThreadRuntime();

			ScopedThreadRuntime(const ScopedThreadRuntime&) = delete;
			ScopedThreadRuntime& operator=(const ScopedThreadRuntime&) = delete;
		};
	}
}