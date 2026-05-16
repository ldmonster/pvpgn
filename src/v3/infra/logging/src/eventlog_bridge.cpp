// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/logging/eventlog_bridge.hpp"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <map>
#include <memory>
#include <mutex>
#include <cstdarg>
#include <cstdio>

namespace pvpgn::infra::logging {

namespace {

// Global logger registry for per-module loggers
std::map<std::string, std::shared_ptr<spdlog::logger>> g_module_loggers;
std::mutex g_loggers_mutex;

/// Get or create a logger for the given module
std::shared_ptr<spdlog::logger> get_module_logger(const char* module) {
    if (!module) {
        module = "unknown";
    }

    std::lock_guard<std::mutex> lock(g_loggers_mutex);

    auto it = g_module_loggers.find(module);
    if (it != g_module_loggers.end()) {
        return it->second;
    }

    // Create a new logger for this module
    auto logger = spdlog::stdout_color_mt(module);
    g_module_loggers[module] = logger;
    return logger;
}

/// Convert LogLevel to spdlog level
spdlog::level::level_enum to_spdlog_level(LogLevel level) noexcept {
    switch (level) {
        case LogLevel::trace:
            return spdlog::level::trace;
        case LogLevel::debug:
            return spdlog::level::debug;
        case LogLevel::info:
            return spdlog::level::info;
        case LogLevel::warn:
            return spdlog::level::warn;
        case LogLevel::error:
            return spdlog::level::err;
        case LogLevel::fatal:
            return spdlog::level::critical;
        default:
            return spdlog::level::info;
    }
}

} // anonymous namespace

void log_message(LogLevel level, const char* module, const char* fmt, ...) {
    if (!fmt) {
        return;
    }

    // Get or create logger for this module
    auto logger = get_module_logger(module);
    auto spdlog_level = to_spdlog_level(level);

    // Format the message using va_args
    // We need to convert printf-style formatting to spdlog format
    va_list args;
    va_start(args, fmt);

    // Use a temporary buffer to format the message
    // This is safe because we're using a fixed-size buffer
    char buffer[4096];
    int written = vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    if (written < 0) {
        logger->log(spdlog_level, "Failed to format log message");
        return;
    }

    // Log the formatted message
    logger->log(spdlog_level, buffer);
}

} // namespace pvpgn::infra::logging
