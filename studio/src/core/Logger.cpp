#include "core/Logger.h"

namespace dalia::studio {

    std::vector<LogEntry> Logger::s_history;
    std::mutex Logger::s_mutex;

    void Logger::Log(LogLevel level, const std::string& category, const std::string& message) {
        Push(LogSystem::Studio, level, category, message);
    }

    void Logger::LogEngine(dalia::LogLevel level, const char* category, const char* message) {
        LogLevel convertedLevel = LogLevel::None;
        switch (level) {
            case dalia::LogLevel::Debug:    convertedLevel = LogLevel::Debug;      break;
            case dalia::LogLevel::Info:     convertedLevel = LogLevel::Info;       break;
            case dalia::LogLevel::Warning:  convertedLevel = LogLevel::Warning;    break;
            case dalia::LogLevel::Error:    convertedLevel = LogLevel::Error;      break;
            case dalia::LogLevel::Critical: convertedLevel = LogLevel::Critical;   break;
            default:                        convertedLevel = LogLevel::None;       break;
        }

        Push(LogSystem::Engine, convertedLevel, category, message);
    }

    const std::vector<LogEntry>& Logger::GetHistory() {
        return s_history;
    }

    void Logger::Clear() {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_history.clear();
    }

    std::mutex& Logger::GetMutex() {
        return s_mutex;
    }

    void Logger::Push(LogSystem system, LogLevel level, const std::string& category, const std::string& message) {
        std::lock_guard<std::mutex> lock(s_mutex);
        LogEntry entry;
        entry.system = system;
        entry.level = level;
        entry.category = category;
        entry.message = message;
        s_history.push_back(entry);
    }
}