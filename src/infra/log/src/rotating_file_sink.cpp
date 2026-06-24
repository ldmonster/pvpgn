// SPDX-License-Identifier: GPL-2.0-or-later

/// @file rotating_file_sink.cpp
/// Implementation of make_rotating_file_logger().
///
/// This translation unit is only compiled when PVPGN_V3_WITH_SPDLOG=ON
/// (it is listed in the infra_log target, which is guarded by that option).

#include "infra/log/rotating_file_sink.hpp"
#include "infra/log/spdlog_logger.hpp"

#include <iostream>
#include <memory>
#include <vector>

#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/spdlog.h>

namespace pvpgn::infra::log {

std::shared_ptr<core::ILogger>
make_rotating_file_logger(const RotatingFileSinkConfig& cfg) {
    try {
        auto sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            cfg.file.string(), cfg.max_size, cfg.max_files);

        auto spd = std::make_shared<spdlog::logger>(
            std::string{cfg.name}, std::move(sink));

        return std::make_shared<SpdlogLogger>(std::move(spd), cfg.level);
    } catch (const std::exception& ex) {
        std::cerr << "[pvpgn] rotating_file_sink: failed to open "
                  << cfg.file << ": " << ex.what()
                  << " — falling back to stderr\n";
    }
    return std::make_shared<core::StreamLogger>(std::cerr, cfg.level);
}

}  // namespace pvpgn::infra::log
