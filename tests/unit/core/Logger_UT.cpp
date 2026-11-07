#include <gtest/gtest.h>
#include "core/Logger.h"

#include <string>
#include <vector>

using namespace dalia;

// A custom sink to capture logs synchronously during tests
struct LogCapture {
    struct Entry {
        LogLevel level;
        std::string context;
        std::string message;
    };
    static std::vector<Entry> entries;

    static void Sink(LogLevel level, const char* context, const char* message) {
        entries.push_back({
            level,
            std::string(context ? context : ""),
            std::string(message ? message : "")
        });
    }

    static void Clear() {
        entries.clear();
    }
};

std::vector<LogCapture::Entry> LogCapture::entries;

class LoggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        LogCapture::Clear();
    }

    void TearDown() override {
        Logger::Shutdown();
    }
};

TEST_F(LoggerTest, BasicLogCaptureAndFormat) {
    Logger::Initialize(LogLevel::Debug, 256);
    Logger::SetSink(LogCapture::Sink);

    // Use the macro to log a formatted string
    DALIA_LOG_INFO(LOG_CTX_CORE, "Hello %s. Number: %d", "World", 42);

    // Because the logger is async, it shouldn't hit the sink until we process it
    EXPECT_EQ(LogCapture::entries.size(), 0);

    Logger::ProcessLogs();

    ASSERT_EQ(LogCapture::entries.size(), 1);
    EXPECT_EQ(LogCapture::entries[0].level, LogLevel::Info);

    // Verify the context was routed correctly
    EXPECT_EQ(LogCapture::entries[0].context, LOG_CTX_CORE);

    // The message should contain the formatted string
    EXPECT_TRUE(LogCapture::entries[0].message.find("Hello World. Number: 42") != std::string::npos);
}

TEST_F(LoggerTest, LogLevelFiltering) {
    // Initialize at WARNING level. Info and Debug should be dropped.
    Logger::Initialize(LogLevel::Warning, 256);
    Logger::SetSink(LogCapture::Sink);

    DALIA_LOG_DEBUG(LOG_CTX_API, "Debug msg");
    DALIA_LOG_INFO(LOG_CTX_API, "Info msg");
    DALIA_LOG_WARN(LOG_CTX_PLATFORM, "Warning msg");
    DALIA_LOG_ERR(LOG_CTX_PLATFORM, "Error msg");

    Logger::ProcessLogs();

    // Only Warning and Error should have made it through
    ASSERT_EQ(LogCapture::entries.size(), 2);

    EXPECT_EQ(LogCapture::entries[0].level, LogLevel::Warning);
    EXPECT_EQ(LogCapture::entries[0].context, LOG_CTX_PLATFORM);

    EXPECT_EQ(LogCapture::entries[1].level, LogLevel::Error);
    EXPECT_EQ(LogCapture::entries[1].context, LOG_CTX_PLATFORM);
}

TEST_F(LoggerTest, LevelNoneDisablesAllLogging) {
    // Level::None should block everything
    Logger::Initialize(LogLevel::None, 256);
    Logger::SetSink(LogCapture::Sink);

    DALIA_LOG_CRIT(LOG_CTX_CORE, "Fatal error should be ignored!");

    Logger::ProcessLogs();

    EXPECT_EQ(LogCapture::entries.size(), 0);
}

TEST_F(LoggerTest, MultipleLogFlushing) {
    Logger::Initialize(LogLevel::Info, 256);
    Logger::SetSink(LogCapture::Sink);

    DALIA_LOG_INFO(LOG_CTX_CORE, "First");
    DALIA_LOG_INFO(LOG_CTX_CORE, "Second");
    DALIA_LOG_INFO(LOG_CTX_CORE, "Third");

    Logger::ProcessLogs();

    ASSERT_EQ(LogCapture::entries.size(), 3);

    // Processing again should do nothing (queue should be empty)
    Logger::ProcessLogs();
    EXPECT_EQ(LogCapture::entries.size(), 3);
}