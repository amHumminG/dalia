#pragma once
#include "dalia/core/LogLevel.h"

namespace dalia {

	static constexpr const char* LOG_CTX_API       = "API";
	static constexpr const char* LOG_CTX_CORE      = "Core";
	static constexpr const char* LOG_CTX_IO        = "I/O";
	static constexpr const char* LOG_CTX_MESSAGING = "Messaging";
	static constexpr const char* LOG_CTX_MIXER     = "Mixer";
	static constexpr const char* LOG_CTX_RESOURCES = "Resources";

	enum class LogLevel : int;

	class Logger {
	public:
		static void Init(LogLevel level, size_t capacity = 256);
		static void Deinit();
		static void SetSink(LogCallback sink);
		static void Log(LogLevel level, const char* context, const char* format, ...);
		static void ProcessLogs();

	private:
		static const char* GetLevelString(LogLevel level);
	};
}

#define DALIA_LOG_DEBUG(context, fmt, ...) \
dalia::Logger::Log(dalia::LogLevel::Debug, context, "%s() - " fmt, __func__, ##__VA_ARGS__)

#define DALIA_LOG_INFO(context, fmt, ...) \
dalia::Logger::Log(dalia::LogLevel::Info, context, "%s() - " fmt, __func__, ##__VA_ARGS__)

#define DALIA_LOG_WARN(context, fmt, ...) \
dalia::Logger::Log(dalia::LogLevel::Warning, context, "%s() - " fmt, __func__, ##__VA_ARGS__)

#define DALIA_LOG_ERR(context, fmt, ...) \
dalia::Logger::Log(dalia::LogLevel::Error, context, "%s() - " fmt, __func__, ##__VA_ARGS__)

#define DALIA_LOG_CRIT(context, fmt, ...) \
dalia::Logger::Log(dalia::LogLevel::Critical, context, "%s() - " fmt, __func__, ##__VA_ARGS__)



