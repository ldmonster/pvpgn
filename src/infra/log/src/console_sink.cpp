// SPDX-License-Identifier: GPL-2.0-or-later

/// @file console_sink.cpp
/// Implementation of make_console_logger() (Plan 10 §3).
///
/// This translation unit is only compiled when PVPGN_V3_WITH_SPDLOG=ON
/// (it is listed in the infra_log target, which is guarded by that option).

#include "infra/log/console_sink.hpp"
#include "infra/log/spdlog_logger.hpp"

#include <iostream>
#include <memory>

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace pvpgn::infra::log {

std::shared_ptr<core::ILogger>
make_console_logger(const ConsoleSinkConfig& cfg) {
    try {
        spdlog::sink_ptr sink;
        if (cfg.use_stderr) {
            sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
        } else {
            sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        }

        auto spd = std::make_shared<spdlog::logger>(
            std::string{cfg.name}, std::move(sink));

        return std::make_shared<SpdlogLogger>(std::move(spd), cfg.level);
    } catch (const std::exception& ex) {
        // Fallback: plain StreamLogger.
        std::cerr << "[pvpgn] console_sink: failed to create spdlog logger: "
                  << ex.what() << " — falling back to StreamLogger\n";
    }
    auto& out = cfg.use_stderr ? std::cerr : std::cout;
    return std::make_shared<core::StreamLogger>(out, cfg.level);
}

}  // namespace pvpgn::infra::log
