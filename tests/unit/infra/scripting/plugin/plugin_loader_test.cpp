// SPDX-License-Identifier: GPL-2.0-or-later
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>

#include "infra/scripting/plugin/plugin_loader.hpp"

using namespace pvpgn::infra::scripting;

TEST_CASE("PluginLoader construction with valid directory", "[infra][plugin][loader]") {
    auto tmp_dir = std::filesystem::temp_directory_path() / "pvpgn_plugin_test";
    std::filesystem::create_directories(tmp_dir);
    
    PluginLoader loader{tmp_dir};
    REQUIRE(loader.plugin_count() == 0);
    
    std::filesystem::remove_all(tmp_dir);
}

TEST_CASE("PluginLoader plugin_count starts at zero", "[infra][plugin][loader]") {
    auto tmp_dir = std::filesystem::temp_directory_path() / "pvpgn_plugin_test_2";
    std::filesystem::create_directories(tmp_dir);
    
    PluginLoader loader{tmp_dir};
    REQUIRE(loader.plugin_count() == 0);
    REQUIRE(loader.get_all_plugins().empty());
    
    std::filesystem::remove_all(tmp_dir);
}

TEST_CASE("PluginLoader is_loaded returns false for non-existent plugin", "[infra][plugin][loader]") {
    auto tmp_dir = std::filesystem::temp_directory_path() / "pvpgn_plugin_test_3";
    std::filesystem::create_directories(tmp_dir);
    
    PluginLoader loader{tmp_dir};
    REQUIRE_FALSE(loader.is_loaded("nonexistent"));
    
    std::filesystem::remove_all(tmp_dir);
}

TEST_CASE("PluginLoader get_plugin returns nullptr for non-existent plugin", "[infra][plugin][loader]") {
    auto tmp_dir = std::filesystem::temp_directory_path() / "pvpgn_plugin_test_4";
    std::filesystem::create_directories(tmp_dir);
    
    PluginLoader loader{tmp_dir};
    REQUIRE(loader.get_plugin("nonexistent") == nullptr);
    
    std::filesystem::remove_all(tmp_dir);
}

TEST_CASE("PluginLoader shutdown_all doesn't crash", "[infra][plugin][loader]") {
    auto tmp_dir = std::filesystem::temp_directory_path() / "pvpgn_plugin_test_5";
    std::filesystem::create_directories(tmp_dir);
    
    PluginLoader loader{tmp_dir};
    loader.shutdown_all();  // Should not crash even with no plugins
    
    std::filesystem::remove_all(tmp_dir);
}
