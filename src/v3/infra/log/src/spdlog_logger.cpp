// SPDX-License-Identifier: GPL-2.0-or-later
#include "infra/log/spdlog_logger.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <vector>

namespace pvpgn::infra::log {

namespace {

spdlog::level::level_enum to_spd(core::LogLevel l) noexcept {
    switch (l) {
        case core::LogLevel::Trace:    return spdlog::level::trace;
        case core::LogLevel::Debug:    return spdlog::level::debug;
        case core::LogLevel::Info:     return spdlog::level::info;
        case core::LogLevel::Warn:     return spdlog::level::warn;
        case core::LogLevel::Error:    return spdlog::level::err;
        case core::LogLevel::Critical: return spdlog::level::critical;
        case core::LogLevel::Off:      return spdlog::level::off;
    }
    return spdlog::level::info;
}

}  // namespace

SpdlogLogger::SpdlogLogger(std::shared_ptr<spdlog::logger> logger,
                           core::LogLevel level)
    : logger_(std::move(logger)), level_(level) {
    if (logger_) logger_->set_level(to_spd(level_));
}

SpdlogLogger::~SpdlogLogger() {
    if (logger_) {
        try { logger_->flush(); } catch (...) {}
    }
}

void SpdlogLogger::log(core::LogLevel level, std::string_view module,
                       std::string_view message) noexcept {
    if (!logger_) return;
    if (static_cast<int>(level) < static_cast<int>(level_)) return;
    try {
        logger_->log(to_spd(level), "[{}] {}", module, message);
    } catch (...) {
        // logging must not throw
    }
}

void SpdlogLogger::set_level(core::LogLevel l) noexcept {
    level_ = l;
    if (logger_) {
        try { logger_->set_level(to_spd(l)); } catch (...) {}
    }
}

std::shared_ptr<core::ILogger> make_spdlog_logger(const SpdlogConfig& cfg) {
    std::vector<spdlog::sink_ptr> sinks;
    if (cfg.stdout_sink) {
        sinks.push_back(
            std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
    }
    if (!cfg.file.empty()) {
        sinks.push_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            cfg.file.string(), cfg.rotate_size, cfg.rotate_files));
    }

    auto logger = std::make_shared<spdlog::logger>(
        cfg.name, sinks.begin(), sinks.end());
    logger->set_pattern("%Y-%m-%dT%H:%M:%S.%e %^%l%$ %v");
    return std::make_shared<SpdlogLogger>(logger, cfg.level);
}

}  // namespace pvpgn::infra::log
