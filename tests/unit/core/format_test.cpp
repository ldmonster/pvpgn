// SPDX-License-Identifier: GPL-2.0-or-later
#include <memory>
#include <sstream>

#include <catch2/catch_test_macros.hpp>

#include "core/format.hpp"

using namespace pvpgn::core;

TEST_CASE("LOG_* macros route through default logger", "[core][format]") {
    std::ostringstream oss;
    set_default_logger(std::make_shared<StreamLogger>(oss, LogLevel::Trace));

#if PVPGN_V3_HAS_STD_FORMAT
    LOG_INFO("test", "n={} name={}", 42, "alice");
    auto s = oss.str();
    REQUIRE(s.find("info")  != std::string::npos);
    REQUIRE(s.find("[test]") != std::string::npos);
    REQUIRE(s.find("n=42")   != std::string::npos);
    REQUIRE(s.find("name=alice") != std::string::npos);
#else
    LOG_INFO("test", "hello");
    REQUIRE(oss.str().find("hello") != std::string::npos);
#endif

    set_default_logger(std::make_shared<NullLogger>());
}

TEST_CASE("LOG_DEBUG is dropped when default level is Info", "[core][format]") {
    std::ostringstream oss;
    set_default_logger(std::make_shared<StreamLogger>(oss, LogLevel::Info));

    LOG_DEBUG("test", "no");
    LOG_INFO("test", "yes");
    REQUIRE(oss.str().find("no")  == std::string::npos);
    REQUIRE(oss.str().find("yes") != std::string::npos);

    set_default_logger(std::make_shared<NullLogger>());
}
