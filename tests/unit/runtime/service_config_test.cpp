// SPDX-License-Identifier: GPL-2.0-or-later
#include <catch2/catch_test_macros.hpp>

#include "runtime/service_host.hpp"

using namespace pvpgn::runtime;

TEST_CASE("ServiceConfig default construction", "[runtime][service_config]") {
    ServiceConfig config;
    REQUIRE(config.service_name.empty());
    REQUIRE(config.config_file.empty());
    REQUIRE_FALSE(config.foreground);
    REQUIRE(config.log_level == "info");
    REQUIRE(config.log_file.empty());
    REQUIRE(config.pid_file.empty());
    REQUIRE(config.work_dir == ".");
    REQUIRE(config.run_as_user.empty());
    REQUIRE(config.run_as_group.empty());
}

TEST_CASE("ServiceConfig field assignment", "[runtime][service_config]") {
    ServiceConfig config;
    config.service_name = "bnetd";
    config.config_file = "/etc/pvpgn/bnetd.conf";
    config.foreground = true;
    config.log_level = "debug";
    config.log_file = "/var/log/pvpgn/bnetd.log";
    config.pid_file = "/var/run/bnetd.pid";
    config.work_dir = "/var/lib/pvpgn";
    config.run_as_user = "pvpgn";
    config.run_as_group = "pvpgn";
    
    REQUIRE(config.service_name == "bnetd");
    REQUIRE(config.config_file == "/etc/pvpgn/bnetd.conf");
    REQUIRE(config.foreground);
    REQUIRE(config.log_level == "debug");
    REQUIRE(config.log_file == "/var/log/pvpgn/bnetd.log");
    REQUIRE(config.pid_file == "/var/run/bnetd.pid");
    REQUIRE(config.work_dir == "/var/lib/pvpgn");
    REQUIRE(config.run_as_user == "pvpgn");
    REQUIRE(config.run_as_group == "pvpgn");
}

TEST_CASE("ServiceConfig foreground defaults to false", "[runtime][service_config]") {
    ServiceConfig config;
    REQUIRE_FALSE(config.foreground);
}

TEST_CASE("ServiceConfig log_level defaults to info", "[runtime][service_config]") {
    ServiceConfig config;
    REQUIRE(config.log_level == "info");
}

TEST_CASE("ServiceConfig work_dir defaults to current directory", "[runtime][service_config]") {
    ServiceConfig config;
    REQUIRE(config.work_dir == ".");
}

TEST_CASE("ServiceConfig can be modified after construction", "[runtime][service_config]") {
    ServiceConfig config;
    config.foreground = true;
    config.log_level = "warn";
    config.work_dir = "/tmp";
    
    REQUIRE(config.foreground);
    REQUIRE(config.log_level == "warn");
    REQUIRE(config.work_dir == "/tmp");
}
