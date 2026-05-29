// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file spdlog_logger.hpp
/// `core::ILogger` implementation backed by spdlog. Phase 1 deliverable:
/// replaces `eventlog()`-style output with a structured, sinkable backend.
///
/// Sinks are wired in `make_spdlog_logger()`:
///   * stdout colour sink (always)
///   * rotating file sink when a path is provided
///
/// Phase 6 will add JSON-line, syslog and Windows Event Log sinks; this
/// header keeps the API stable for that.

#include <filesystem>
#include <memory>
#include <string>

#include "core/logging.hpp"

namespace spdlog { class logger; }

namespace pvpgn::infra::log {

struct SpdlogConfig {
    core::LogLevel level         = core::LogLevel::Info;
    std::string    name          = "pvpgn";
    /// If empty, file sink is omitted.
    std::filesystem::path file;
    std::size_t    rotate_size   = 10 * 1024 * 1024;
    std::size_t    rotate_files  = 5;
    bool           stdout_sink   = true;
    bool           pattern_color = true;
};

class SpdlogLogger final : public core::ILogger {
public:
    explicit SpdlogLogger(std::shared_ptr<spdlog::logger> logger,
                          core::LogLevel level);
    ~SpdlogLogger() override;

    void log(core::LogLevel level, std::string_view module,
             std::string_view message) noexcept override;
    core::LogLevel level() const noexcept override { return level_; }
    void set_level(core::LogLevel) noexcept override;

private:
    std::shared_ptr<spdlog::logger> logger_;
    core::LogLevel                  level_;
};

std::shared_ptr<core::ILogger> make_spdlog_logger(const SpdlogConfig& cfg);

}  // namespace pvpgn::infra::log
