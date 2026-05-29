// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// Structured Logging Integration
///
/// Provides structured logging using spdlog with support for multiple log levels,
/// file output, and key-value pair logging.

#include <string>
#include <string_view>
#include <filesystem>
#include <optional>
#include <memory>

namespace pvpgn::runtime {

/// Log level enumeration
enum class LogLevel {
    trace,
    debug,
    info,
    warn,
    error,
    critical
};

/// Service logger for structured logging
class ServiceLogger {
public:
    /// Initialize the logger
    /// @param service_name Name of the service
    /// @param level Log level
    /// @param log_file Optional log file path (empty = stdout)
    static void init(std::string_view service_name, LogLevel level,
                     const std::optional<std::filesystem::path>& log_file = std::nullopt);
    
    /// Set the log level
    /// @param level New log level
    static void set_level(LogLevel level);
    
    /// Flush all pending log messages
    static void flush();
    
    /// Log a message with structured key-value pairs
    /// @param level Log level
    /// @param msg Log message
    /// @param args Key-value pairs (variadic)
    template<typename... Args>
    static void log(LogLevel level, std::string_view msg, Args&&... args);
    
    /// Log at trace level
    template<typename... Args>
    static void trace(std::string_view msg, Args&&... args) {
        log(LogLevel::trace, msg, std::forward<Args>(args)...);
    }
    
    /// Log at debug level
    template<typename... Args>
    static void debug(std::string_view msg, Args&&... args) {
        log(LogLevel::debug, msg, std::forward<Args>(args)...);
    }
    
    /// Log at info level
    template<typename... Args>
    static void info(std::string_view msg, Args&&... args) {
        log(LogLevel::info, msg, std::forward<Args>(args)...);
    }
    
    /// Log at warn level
    template<typename... Args>
    static void warn(std::string_view msg, Args&&... args) {
        log(LogLevel::warn, msg, std::forward<Args>(args)...);
    }
    
    /// Log at error level
    template<typename... Args>
    static void error(std::string_view msg, Args&&... args) {
        log(LogLevel::error, msg, std::forward<Args>(args)...);
    }
    
    /// Log at critical level
    template<typename... Args>
    static void critical(std::string_view msg, Args&&... args) {
        log(LogLevel::critical, msg, std::forward<Args>(args)...);
    }
    
    /// Shutdown the logger
    static void shutdown();

private:
    ServiceLogger() = delete;
    ~ServiceLogger() = delete;
};

// Convenience macros
#define SVC_LOG_TRACE(msg, ...) pvpgn::runtime::ServiceLogger::trace(msg, ##__VA_ARGS__)
#define SVC_LOG_DEBUG(msg, ...) pvpgn::runtime::ServiceLogger::debug(msg, ##__VA_ARGS__)
#define SVC_LOG_INFO(msg, ...) pvpgn::runtime::ServiceLogger::info(msg, ##__VA_ARGS__)
#define SVC_LOG_WARN(msg, ...) pvpgn::runtime::ServiceLogger::warn(msg, ##__VA_ARGS__)
#define SVC_LOG_ERROR(msg, ...) pvpgn::runtime::ServiceLogger::error(msg, ##__VA_ARGS__)
#define SVC_LOG_CRITICAL(msg, ...) pvpgn::runtime::ServiceLogger::critical(msg, ##__VA_ARGS__)

} // namespace pvpgn::runtime
