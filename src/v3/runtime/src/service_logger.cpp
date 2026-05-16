// SPDX-License-Identifier: GPL-2.0-or-later
#include "runtime/service_logger.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/pattern_formatter.h>
#include <memory>
#include <mutex>

namespace pvpgn::runtime {

namespace {
    std::shared_ptr<spdlog::logger> g_logger;
    std::mutex g_logger_mutex;
    bool g_initialized = false;
}

void ServiceLogger::init(std::string_view service_name, LogLevel level,
                         const std::optional<std::filesystem::path>& log_file)
{
    std::lock_guard<std::mutex> lock(g_logger_mutex);
    
    if (g_initialized) {
        return;
    }
    
    std::vector<spdlog::sink_ptr> sinks;
    
    // Always add console sink
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    sinks.push_back(console_sink);
    
    // Add file sink if specified
    if (log_file && !log_file->empty()) {
        try {
            auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                log_file->string(), 10 * 1024 * 1024, 3);
            sinks.push_back(file_sink);
        } catch (const spdlog::spdlog_ex& ex) {
            // If file sink fails, continue with console only
        }
    }
    
    // Create logger with all sinks
    g_logger = std::make_shared<spdlog::logger>(
        std::string(service_name), sinks.begin(), sinks.end());
    
    // Set pattern
    g_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] %v");
    
    // Set level
    switch (level) {
        case LogLevel::trace:
            g_logger->set_level(spdlog::level::trace);
            break;
        case LogLevel::debug:
            g_logger->set_level(spdlog::level::debug);
            break;
        case LogLevel::info:
            g_logger->set_level(spdlog::level::info);
            break;
        case LogLevel::warn:
            g_logger->set_level(spdlog::level::warn);
            break;
        case LogLevel::error:
            g_logger->set_level(spdlog::level::err);
            break;
        case LogLevel::critical:
            g_logger->set_level(spdlog::level::critical);
            break;
    }
    
    // Flush on error and critical
    g_logger->flush_on(spdlog::level::err);
    
    // Register globally
    spdlog::register_logger(g_logger);
    
    g_initialized = true;
}

void ServiceLogger::set_level(LogLevel level)
{
    std::lock_guard<std::mutex> lock(g_logger_mutex);
    
    if (!g_logger) {
        return;
    }
    
    switch (level) {
        case LogLevel::trace:
            g_logger->set_level(spdlog::level::trace);
            break;
        case LogLevel::debug:
            g_logger->set_level(spdlog::level::debug);
            break;
        case LogLevel::info:
            g_logger->set_level(spdlog::level::info);
            break;
        case LogLevel::warn:
            g_logger->set_level(spdlog::level::warn);
            break;
        case LogLevel::error:
            g_logger->set_level(spdlog::level::err);
            break;
        case LogLevel::critical:
            g_logger->set_level(spdlog::level::critical);
            break;
    }
}

void ServiceLogger::flush()
{
    std::lock_guard<std::mutex> lock(g_logger_mutex);
    
    if (g_logger) {
        g_logger->flush();
    }
}

template<typename... Args>
void ServiceLogger::log(LogLevel level, std::string_view msg, Args&&... args)
{
    std::lock_guard<std::mutex> lock(g_logger_mutex);
    
    if (!g_logger) {
        return;
    }
    
    // For now, just log the message without structured fields
    // In a real implementation, we would format key-value pairs
    switch (level) {
        case LogLevel::trace:
            g_logger->trace(msg);
            break;
        case LogLevel::debug:
            g_logger->debug(msg);
            break;
        case LogLevel::info:
            g_logger->info(msg);
            break;
        case LogLevel::warn:
            g_logger->warn(msg);
            break;
        case LogLevel::error:
            g_logger->error(msg);
            break;
        case LogLevel::critical:
            g_logger->critical(msg);
            break;
    }
}

void ServiceLogger::shutdown()
{
    std::lock_guard<std::mutex> lock(g_logger_mutex);
    
    if (g_logger) {
        g_logger->flush();
        spdlog::drop_all();
        g_logger.reset();
        g_initialized = false;
    }
}

// Explicit template instantiation for common cases
template void ServiceLogger::log<>(LogLevel, std::string_view);

} // namespace pvpgn::runtime
