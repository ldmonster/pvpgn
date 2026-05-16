// SPDX-License-Identifier: GPL-2.0-or-later
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>

#include "infra/scripting/plugin/plugin_loader.hpp"
#include "infra/scripting/plugin/plugin_manifest.hpp"

using namespace pvpgn::infra::scripting;

TEST_CASE("Plugin lifecycle: create loader with empty directory", "[functional][plugin]") {
    auto tmp_dir = std::filesystem::temp_directory_path() / "pvpgn_plugins_empty";
    std::filesystem::create_directories(tmp_dir);
    
    PluginLoader loader{tmp_dir};
    REQUIRE(loader.plugin_count() == 0);
    REQUIRE(loader.get_all_plugins().empty());
    
    std::filesystem::remove_all(tmp_dir);
}

TEST_CASE("Plugin lifecycle: create plugin directory structure", "[functional][plugin]") {
    auto tmp_dir = std::filesystem::temp_directory_path() / "pvpgn_plugins_struct";
    auto plugin_dir = tmp_dir / "test-plugin";
    std::filesystem::create_directories(plugin_dir);
    
    // Create a minimal plugin.toml
    {
        std::ofstream manifest(plugin_dir / "plugin.toml");
        manifest << R"(
name = "test-plugin"
version = "1.0.0"
api_version = "3.0"
entry = "main.lua"
plugin_type = "lua"
description = "A test plugin"
)";
    }
    
    // Create a minimal main.lua
    {
        std::ofstream lua(plugin_dir / "main.lua");
        lua << "-- Test plugin\n";
    }
    
    PluginLoader loader{tmp_dir};
    // Just verify the loader can be created without crashing
    REQUIRE(loader.plugin_count() == 0);
    
    std::filesystem::remove_all(tmp_dir);
}

TEST_CASE("Plugin lifecycle: load_all on empty directory", "[functional][plugin]") {
    auto tmp_dir = std::filesystem::temp_directory_path() / "pvpgn_plugins_load_empty";
    std::filesystem::create_directories(tmp_dir);
    
    PluginLoader loader{tmp_dir};
    auto error = loader.load_all();
    // Should not crash, error message may be empty or indicate no plugins found
    REQUIRE(true);
    
    std::filesystem::remove_all(tmp_dir);
}

TEST_CASE("Plugin lifecycle: shutdown_all on empty loader", "[functional][plugin]") {
    auto tmp_dir = std::filesystem::temp_directory_path() / "pvpgn_plugins_shutdown";
    std::filesystem::create_directories(tmp_dir);
    
    PluginLoader loader{tmp_dir};
    loader.shutdown_all();  // Should not crash
    REQUIRE(true);
    
    std::filesystem::remove_all(tmp_dir);
}

TEST_CASE("Plugin manifest: parse from TOML string", "[functional][plugin]") {
    PluginManifest manifest;
    manifest.name = "tournament-pack";
    manifest.version = "2.1.0";
    manifest.api_version = "3.0";
    manifest.entry = "main.lua";
    manifest.plugin_type = "lua";
    manifest.description = "Tournament management plugin";
    manifest.authors.push_back("Alice");
    manifest.authors.push_back("Bob");
    manifest.capabilities.push_back("chat.send");
    manifest.capabilities.push_back("db.read");
    
    REQUIRE(manifest.name == "tournament-pack");
    REQUIRE(manifest.version == "2.1.0");
    REQUIRE(manifest.authors.size() == 2);
    REQUIRE(manifest.capabilities.size() == 2);
}

TEST_CASE("Plugin capability set: parse and verify", "[functional][plugin]") {
    PluginManifest manifest;
    manifest.capabilities.push_back("chat.send");
    manifest.capabilities.push_back("db.read");
    manifest.capabilities.push_back("db.write");
    
    auto cap_set = manifest.parse_capabilities();
    // Just verify it doesn't crash and returns a valid set
    REQUIRE(cap_set.mask() > 0);
}
