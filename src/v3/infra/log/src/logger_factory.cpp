// SPDX-License-Identifier: GPL-2.0-or-later
#include "infra/log/logger_factory.hpp"
#include "core/logging.hpp"
#include "infra/log/spdlog_logger.hpp"
#include <iostream>

namespace pvpgn::infra::log {

std::shared_ptr<core::ILogger> make_and_install_logger(
    const infra::config::LogConfig& log_cfg,
    std::string_view logger_name) noexcept
{
    SpdlogConfig cfg;
    cfg.level         = log_cfg.level;
    cfg.name          = std::string(logger_name);
    cfg.file          = log_cfg.file;
    cfg.rotate_size   = log_cfg.rotate_size;
    cfg.rotate_files  = log_cfg.rotate_files;
    cfg.stdout_sink   = log_cfg.stdout_sink;
    cfg.pattern_color = true;

    auto logger = make_spdlog_logger(cfg);
    if (!logger) {
        // Fallback: stderr stream logger
        logger = std::make_shared<core::StreamLogger>(std::cerr, core::LogLevel::Info);
    }
    core::set_default_logger(logger);
    return logger;
}

} // namespace pvpgn::infra::log
