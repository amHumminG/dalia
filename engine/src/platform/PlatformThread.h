#pragma once

#include <string>

namespace dalia {

	enum class ThreadPriority {
		Normal,
		High,
		TimeCritical
	};

	namespace PlatformThread {

		void SetCurrentThreadPriority(ThreadPriority priority);

		void SetCurrentThreadName(const std::string& name);

		struct ScopedThreadRuntime {
			ScopedThreadRuntime();
			~ScopedThreadRuntime();

			ScopedThreadRuntime(const ScopedThreadRuntime&) = delete;
			ScopedThreadRuntime& operator=(const ScopedThreadRuntime&) = delete;
		};
	}
}