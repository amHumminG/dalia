#pragma once
#include "dalia/core/LogLevel.h"

namespace dalia {

	enum class LogLevel : int;

	class Logger {
	public:
		static void Init(LogLevel level, size_t capacity = 256);
		static void Deinit();
		static void SetSink(LogCallback sink);
		static void Log(LogLevel level, const char* category, const char* format, ...);
		static void ProcessLogs();

	private:
		static const char* GetLevelString(LogLevel level);
	};
}

#define DALIA_LOG_DEBUG(category, fmt, ...) \
dalia::Logger::Log(dalia::LogLevel::Debug, category, "%s() - " fmt, __func__, ##__VA_ARGS__)

#define DALIA_LOG_INFO(category, fmt, ...) \
dalia::Logger::Log(dalia::LogLevel::Info, category, "%s() - " fmt, __func__, ##__VA_ARGS__)

#define DALIA_LOG_WARN(category, fmt, ...) \
dalia::Logger::Log(dalia::LogLevel::Warning, category, "%s() - " fmt, __func__, ##__VA_ARGS__)

#define DALIA_LOG_ERROR(category, fmt, ...) \
dalia::Logger::Log(dalia::LogLevel::Error, category, "%s() - " fmt, __func__, ##__VA_ARGS__)

#define DALIA_LOG_CRITICAL(category, fmt, ...) \
dalia::Logger::Log(dalia::LogLevel::Critical, category, "%s() - " fmt, __func__, ##__VA_ARGS__)



