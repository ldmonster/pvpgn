// SPDX-License-Identifier: GPL-2.0-or-later
#include <memory>
#include <sstream>

#include <catch2/catch_test_macros.hpp>

#include "core/logging.hpp"

using namespace pvpgn::core;

TEST_CASE("default logger is replaceable and writes", "[core][logging]") {
    std::ostringstream oss;
    auto sink = std::make_shared<StreamLogger>(oss, LogLevel::Trace);
    set_default_logger(sink);

    log(LogLevel::Info, "test", "hello");
    auto s = oss.str();
    REQUIRE(s.find("info") != std::string::npos);
    REQUIRE(s.find("[test]") != std::string::npos);
    REQUIRE(s.find("hello") != std::string::npos);

    // Restore.
    set_default_logger(std::make_shared<NullLogger>());
}

TEST_CASE("level filtering drops below-min messages", "[core][logging]") {
    std::ostringstream oss;
    auto sink = std::make_shared<StreamLogger>(oss, LogLevel::Warn);
    set_default_logger(sink);

    log(LogLevel::Debug, "m", "no");
    log(LogLevel::Error, "m", "yes");
    REQUIRE(oss.str().find("no")  == std::string::npos);
    REQUIRE(oss.str().find("yes") != std::string::npos);

    set_default_logger(std::make_shared<NullLogger>());
}
