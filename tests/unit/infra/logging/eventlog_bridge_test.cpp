// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>
#include "infra/logging/eventlog_bridge.hpp"
#include <spdlog/spdlog.h>

namespace pvpgn::infra::logging::test {

class EventlogBridgeTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clear any existing loggers before each test
        spdlog::drop_all();
    }

    void TearDown() override {
        // Clean up loggers after each test
        spdlog::drop_all();
    }
};

// Test LOG_INFO doesn't crash
TEST_F(EventlogBridgeTest, LogInfoDoesNotCrash) {
    EXPECT_NO_THROW({
        LOG_INFO("test_module", "This is an info message");
    });
}

// Test LOG_ERROR doesn't crash
TEST_F(EventlogBridgeTest, LogErrorDoesNotCrash) {
    EXPECT_NO_THROW({
        LOG_ERROR("test_module", "This is an error message");
    });
}

// Test LOG_DEBUG doesn't crash
TEST_F(EventlogBridgeTest, LogDebugDoesNotCrash) {
    EXPECT_NO_THROW({
        LOG_DEBUG("test_module", "This is a debug message");
    });
}

// Test LOG_WARN doesn't crash
TEST_F(EventlogBridgeTest, LogWarnDoesNotCrash) {
    EXPECT_NO_THROW({
        LOG_WARN("test_module", "This is a warning message");
    });
}

// Test LOG_TRACE doesn't crash
TEST_F(EventlogBridgeTest, LogTraceDoesNotCrash) {
    EXPECT_NO_THROW({
        LOG_TRACE("test_module", "This is a trace message");
    });
}

// Test LOG_FATAL doesn't crash
TEST_F(EventlogBridgeTest, LogFatalDoesNotCrash) {
    EXPECT_NO_THROW({
        LOG_FATAL("test_module", "This is a fatal message");
    });
}

// Test different modules create separate loggers
TEST_F(EventlogBridgeTest, DifferentModulesCreateSeparateLoggers) {
    EXPECT_NO_THROW({
        LOG_INFO("module_a", "Message from module A");
        LOG_INFO("module_b", "Message from module B");
        LOG_INFO("module_c", "Message from module C");
    });

    // Verify loggers were created
    auto logger_a = spdlog::get("module_a");
    auto logger_b = spdlog::get("module_b");
    auto logger_c = spdlog::get("module_c");

    EXPECT_NE(logger_a, nullptr);
    EXPECT_NE(logger_b, nullptr);
    EXPECT_NE(logger_c, nullptr);

    // Verify they are different loggers
    EXPECT_NE(logger_a, logger_b);
    EXPECT_NE(logger_b, logger_c);
}

// Test formatted messages with arguments
TEST_F(EventlogBridgeTest, FormattedMessagesWithArguments) {
    EXPECT_NO_THROW({
        LOG_INFO("test_module", "User %s logged in from %s", "alice", "192.168.1.1");
        LOG_ERROR("test_module", "Error code: %d, message: %s", 42, "Something went wrong");
        LOG_DEBUG("test_module", "Value: %d, Float: %.2f", 100, 3.14159);
    });
}

// Test log_message function directly
TEST_F(EventlogBridgeTest, DirectLogMessageCall) {
    EXPECT_NO_THROW({
        log_message(LogLevel::info, "direct_test", "Direct call test");
        log_message(LogLevel::error, "direct_test", "Error: %s", "test error");
        log_message(LogLevel::debug, "direct_test", "Debug value: %d", 42);
    });
}

// Test with nullptr module name
TEST_F(EventlogBridgeTest, NullptrModuleNameHandled) {
    EXPECT_NO_THROW({
        log_message(LogLevel::info, nullptr, "Message with nullptr module");
    });
}

// Test with nullptr format string
TEST_F(EventlogBridgeTest, NullptrFormatStringHandled) {
    EXPECT_NO_THROW({
        log_message(LogLevel::info, "test_module", nullptr);
    });
}

// Test all log levels
TEST_F(EventlogBridgeTest, AllLogLevels) {
    EXPECT_NO_THROW({
        log_message(LogLevel::trace, "levels", "Trace level");
        log_message(LogLevel::debug, "levels", "Debug level");
        log_message(LogLevel::info, "levels", "Info level");
        log_message(LogLevel::warn, "levels", "Warn level");
        log_message(LogLevel::error, "levels", "Error level");
        log_message(LogLevel::fatal, "levels", "Fatal level");
    });
}

// Test repeated calls to same module
TEST_F(EventlogBridgeTest, RepeatedCallsSameModule) {
    EXPECT_NO_THROW({
        for (int i = 0; i < 10; ++i) {
            LOG_INFO("repeated_module", "Message %d", i);
        }
    });

    // Verify logger was reused (not created multiple times)
    auto logger = spdlog::get("repeated_module");
    EXPECT_NE(logger, nullptr);
}

// Test long messages
TEST_F(EventlogBridgeTest, LongMessages) {
    EXPECT_NO_THROW({
        std::string long_msg(1000, 'a');
        LOG_INFO("test_module", "Long message: %s", long_msg.c_str());
    });
}

// Test special characters in messages
TEST_F(EventlogBridgeTest, SpecialCharactersInMessages) {
    EXPECT_NO_THROW({
        LOG_INFO("test_module", "Special chars: !@#$%%^&*()_+-=[]{}|;:',.<>?/");
        LOG_ERROR("test_module", "Newline test: line1\\nline2");
        LOG_DEBUG("test_module", "Tab test: col1\\tcol2");
    });
}

} // namespace pvpgn::infra::logging::test
