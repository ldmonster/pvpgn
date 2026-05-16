// SPDX-License-Identifier: GPL-2.0-or-later
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>

#include "infra/config/server_config.hpp"

using namespace pvpgn;
using namespace std::string_view_literals;

TEST_CASE("Config roundtrip: write TOML, load, verify", "[functional][config]") {
    auto tmp_file = std::filesystem::temp_directory_path() / "pvpgn_config_roundtrip.toml";
    
    // Write a test config file
    {
        std::ofstream out(tmp_file);
        out << R"(
[server]
name = "TestRealm"
script_dir = "/var/pvpgn/lua"

[network]
bind_addr = "127.0.0.1"
port = 6200

[log]
level = "debug"
file = "/var/log/pvpgn.log"
rotate_size = 1048576
rotate_files = 3
stdout = false

[storage]
driver = "sqlite"
dsn = "file:pvpgn.db"
pool = 8
)";
    }
    
    // Load the config
    auto result = infra::config::load_server_config(tmp_file);
    REQUIRE(result.has_value());
    
    auto& config = result.value();
    
    // Verify all values match what we wrote
    REQUIRE(config.servername == "TestRealm");
    REQUIRE(config.script_dir == "/var/pvpgn/lua");
    REQUIRE(config.network.bind_addr == "127.0.0.1");
    REQUIRE(config.network.port == 6200);
    REQUIRE(config.log.level == core::LogLevel::Debug);
    REQUIRE(config.log.file == "/var/log/pvpgn.log");
    REQUIRE(config.log.rotate_size == 1048576u);
    REQUIRE(config.log.rotate_files == 3u);
    REQUIRE_FALSE(config.log.stdout_sink);
    REQUIRE(config.storage.driver == "sqlite");
    REQUIRE(config.storage.dsn == "file:pvpgn.db");
    REQUIRE(config.storage.pool == 8u);
    
    // Cleanup
    std::filesystem::remove(tmp_file);
}

TEST_CASE("Config roundtrip: minimal config", "[functional][config]") {
    auto tmp_file = std::filesystem::temp_directory_path() / "pvpgn_config_minimal.toml";
    
    // Write a minimal config
    {
        std::ofstream out(tmp_file);
        out << "";  // Empty file should use all defaults
    }
    
    auto result = infra::config::load_server_config(tmp_file);
    REQUIRE(result.has_value());
    
    auto& config = result.value();
    REQUIRE(config.servername == "PvPGN");
    REQUIRE(config.network.bind_addr == "0.0.0.0");
    REQUIRE(config.network.port == 6112);
    REQUIRE(config.log.level == core::LogLevel::Info);
    REQUIRE(config.storage.driver == "file");
    
    std::filesystem::remove(tmp_file);
}

TEST_CASE("Config roundtrip: partial config with defaults", "[functional][config]") {
    auto tmp_file = std::filesystem::temp_directory_path() / "pvpgn_config_partial.toml";
    
    // Write a partial config
    {
        std::ofstream out(tmp_file);
        out << R"(
[server]
name = "PartialRealm"

[network]
port = 7000
)";
    }
    
    auto result = infra::config::load_server_config(tmp_file);
    REQUIRE(result.has_value());
    
    auto& config = result.value();
    REQUIRE(config.servername == "PartialRealm");
    REQUIRE(config.network.bind_addr == "0.0.0.0");  // Default
    REQUIRE(config.network.port == 7000);  // Overridden
    REQUIRE(config.log.level == core::LogLevel::Info);  // Default
    
    std::filesystem::remove(tmp_file);
}

TEST_CASE("Config roundtrip: file not found", "[functional][config]") {
    auto result = infra::config::load_server_config("/nonexistent/path/config.toml");
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code() == core::StatusCode::NotFound);
}

TEST_CASE("Config roundtrip: invalid TOML syntax", "[functional][config]") {
    auto tmp_file = std::filesystem::temp_directory_path() / "pvpgn_config_invalid.toml";
    
    // Write invalid TOML
    {
        std::ofstream out(tmp_file);
        out << "this = is = not = valid = toml";
    }
    
    auto result = infra::config::load_server_config(tmp_file);
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code() == core::StatusCode::InvalidArgument);
    
    std::filesystem::remove(tmp_file);
}
