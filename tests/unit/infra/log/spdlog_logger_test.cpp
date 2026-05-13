// SPDX-License-Identifier: GPL-2.0-or-later
#include <filesystem>
#include <memory>

#include <catch2/catch_test_macros.hpp>

#include "core/logging.hpp"
#include "infra/log/spdlog_logger.hpp"

using namespace pvpgn;

TEST_CASE("spdlog logger writes to stdout sink without crashing",
          "[infra][log]") {
    infra::log::SpdlogConfig cfg;
    cfg.level       = core::LogLevel::Debug;
    cfg.name        = "test";
    cfg.stdout_sink = true;
    auto logger = infra::log::make_spdlog_logger(cfg);
    REQUIRE(logger);
    REQUIRE(logger->level() == core::LogLevel::Debug);

    logger->log(core::LogLevel::Info,  "auth",    "user joined");
    logger->log(core::LogLevel::Warn,  "storage", "slow query");
    logger->log(core::LogLevel::Error, "net",     "disconnect");
}

TEST_CASE("spdlog logger writes to rotating file sink", "[infra][log]") {
    auto tmpdir = std::filesystem::temp_directory_path() /
                  "pvpgn_v3_spdlog_test";
    std::filesystem::create_directories(tmpdir);
    auto file = tmpdir / "test.log";
    std::filesystem::remove(file);

    infra::log::SpdlogConfig cfg;
    cfg.level       = core::LogLevel::Trace;
    cfg.stdout_sink = false;
    cfg.file        = file;
    {
        auto logger = infra::log::make_spdlog_logger(cfg);
        REQUIRE(logger);
        logger->log(core::LogLevel::Info, "test", "hello");
    }
    REQUIRE(std::filesystem::exists(file));
    REQUIRE(std::filesystem::file_size(file) > 0);
}

TEST_CASE("spdlog logger level filtering", "[infra][log]") {
    infra::log::SpdlogConfig cfg;
    cfg.level       = core::LogLevel::Warn;
    cfg.stdout_sink = true;
    auto logger = infra::log::make_spdlog_logger(cfg);

    logger->log(core::LogLevel::Debug, "m", "below");
    logger->log(core::LogLevel::Error, "m", "above");
    logger->set_level(core::LogLevel::Trace);
    REQUIRE(logger->level() == core::LogLevel::Trace);
}
