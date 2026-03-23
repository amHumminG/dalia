#define _CRT_SECURE_NO_WARNINGS

#include "Logger.h"
#include "MPSCRingBuffer.h"
#include <cstdarg>

namespace dalia {

	struct LogEntry {
		LogLevel level;
		char category[16];
		char message[512];
	};

	static LogLevel m_logLevel = LogLevel::Warning;
	static std::unique_ptr<MPSCRingBuffer<LogEntry>> m_buffer = nullptr;
	static LogCallback m_sink = nullptr;

	void Logger::Init(LogLevel level, size_t capacity) {
		m_logLevel = static_cast<LogLevel>(level);
		m_buffer = std::make_unique<MPSCRingBuffer<LogEntry>>(capacity);
	}

	void Logger::Deinit() {
		ProcessLogs();
		m_buffer.reset();
	}

	void Logger::SetSink(LogCallback sink) {
		m_sink = std::move(sink);
	}

	void Logger::Log(LogLevel level, const char* context, const char* format, ...) {
		if (!m_buffer) return;

		if (static_cast<int>(level) < static_cast<int>(m_logLevel)) return;

		LogEntry entry;
		entry.level = level;
		strncpy(entry.category, context, sizeof(entry.category) - 1);
		entry.category[sizeof(entry.category) - 1] = '\0';

		va_list args;
		va_start(args, format);
		vsnprintf(entry.message, sizeof(entry.message), format, args);
		va_end(args);

		m_buffer->Push(entry);
	}

	void Logger::ProcessLogs() {
		if (!m_buffer) return;

		LogEntry entry;
		bool didPrint = false;
		while (m_buffer->Pop(entry)) {
			if (m_sink) {
				m_sink(entry.level, entry.category, entry.message);
			}
			std::printf("[DALIA %s] [%s] %s\n", GetLevelString(entry.level), entry.category, entry.message);
			didPrint = true;
		}

		if (didPrint) std::fflush(stdout);
	}
	const char* Logger::GetLevelString(LogLevel level) {
		switch (level) {
		case LogLevel::Debug:		return "DEBUG";
		case LogLevel::Info:		return "INFO";
		case LogLevel::Warning:		return "WARNING";
		case LogLevel::Error:		return "ERROR";
		case LogLevel::Critical:	return "CRITICAL";
		default:					return "NONE";
		}
	}
}