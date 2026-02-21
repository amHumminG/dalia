#pragma once
#include "dalia/LogLevel.h"

namespace dalia {

	enum class LogLevel : int;

	class Logger {
	public:
		static void Init(LogLevel level, size_t capacity = 256);
		static void Log(LogLevel level, const char* category, const char* format, ...);
		static void ProcessLogs();

	private:
		static const char* GetLevelString(LogLevel level);
	};
}



