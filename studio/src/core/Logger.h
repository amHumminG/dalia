#pragma once
#include "dalia/LogLevel.h"
#include <string>
#include <vector>
#include <format>
#include <mutex>

namespace dalia::studio {

    enum class LogSystem : int {
        Studio = 0,
        Engine = 1
    };

    enum class LogLevel : int {
        Debug		= 0,
        Info		= 1,
        Warning		= 2,
        Error		= 3,
        Critical	= 4,
        None		= 5,
    };

    struct LogEntry {
        LogSystem system;
        LogLevel level;
        std::string category;
        std::string message;
    };

    class Logger {
    public:
        static void Log(LogLevel level, const std::string& category, const std::string& message);

        // This function is only used as a callback for the internal engine instance
        static void LogEngine(dalia::LogLevel, const char* category, const char* message);

        static const std::vector<LogEntry>& GetHistory();
        static void Clear();
        static std::mutex& GetMutex(); // Do we need this?

    private:
        static void Push(LogSystem system, LogLevel level, const std::string& category, const std::string& message);

        static std::vector<LogEntry> s_history;
        static std::mutex s_mutex;
    };
}