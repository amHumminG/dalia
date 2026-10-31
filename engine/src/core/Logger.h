#pragma once
#include "dalia/LogLevel.h"

namespace dalia {

	static constexpr const char* LOG_CTX_API       = "API";
	static constexpr const char* LOG_CTX_BACKEND   = "Device";
	static constexpr const char* LOG_CTX_CORE      = "Core";
	static constexpr const char* LOG_CTX_ASYNC     = "Async";
	static constexpr const char* LOG_CTX_MIXER     = "Mixer";

	enum class LogLevel : int;

	class Logger {
	public:
		static void Initialize(LogLevel level, size_t capacity = 256);
		static void Shutdown();
		static void SetSink(LogCallback sink);
		static void Log(LogLevel level, const char* context, const char* format, ...);
		static void ProcessLogs();

	private:
		static const char* GetLevelString(LogLevel level);
	};
}

#define DALIA_LOG_DEBUG(context, fmt, ...) \
dalia::Logger::Log(dalia::LogLevel::Debug, context, fmt, ##__VA_ARGS__)

#define DALIA_LOG_INFO(context, fmt, ...) \
dalia::Logger::Log(dalia::LogLevel::Info, context, fmt, ##__VA_ARGS__)

#define DALIA_LOG_WARN(context, fmt, ...) \
dalia::Logger::Log(dalia::LogLevel::Warning, context, fmt, ##__VA_ARGS__)

#define DALIA_LOG_ERR(context, fmt, ...) \
dalia::Logger::Log(dalia::LogLevel::Error, context, fmt, ##__VA_ARGS__)

#define DALIA_LOG_CRIT(context, fmt, ...) \
dalia::Logger::Log(dalia::LogLevel::Critical, context, fmt, ##__VA_ARGS__)



