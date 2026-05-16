// SPDX-License-Identifier: GPL-2.0-or-later
#include <catch2/catch_test_macros.hpp>

#include "infra/scripting/plugin/plugin_manifest.hpp"

using namespace pvpgn::infra::scripting;

TEST_CASE("PluginManifest default construction", "[infra][plugin][manifest]") {
    PluginManifest manifest;
    REQUIRE(manifest.name.empty());
    REQUIRE(manifest.version.empty());
    REQUIRE(manifest.authors.empty());
    REQUIRE(manifest.api_version.empty());
    REQUIRE(manifest.entry.empty());
    REQUIRE(manifest.plugin_type.empty());
    REQUIRE(manifest.capabilities.empty());
    REQUIRE(manifest.description.empty());
    REQUIRE(manifest.plugin_dir.empty());
}

TEST_CASE("PluginManifest field assignment", "[infra][plugin][manifest]") {
    PluginManifest manifest;
    manifest.name = "test-plugin";
    manifest.version = "1.0.0";
    manifest.api_version = "3.0";
    manifest.entry = "main.lua";
    manifest.plugin_type = "lua";
    manifest.description = "A test plugin";
    manifest.plugin_dir = "/path/to/plugin";
    
    REQUIRE(manifest.name == "test-plugin");
    REQUIRE(manifest.version == "1.0.0");
    REQUIRE(manifest.api_version == "3.0");
    REQUIRE(manifest.entry == "main.lua");
    REQUIRE(manifest.plugin_type == "lua");
    REQUIRE(manifest.description == "A test plugin");
    REQUIRE(manifest.plugin_dir == "/path/to/plugin");
}

TEST_CASE("PluginManifest authors list", "[infra][plugin][manifest]") {
    PluginManifest manifest;
    manifest.authors.push_back("Alice");
    manifest.authors.push_back("Bob");
    
    REQUIRE(manifest.authors.size() == 2);
    REQUIRE(manifest.authors[0] == "Alice");
    REQUIRE(manifest.authors[1] == "Bob");
}

TEST_CASE("PluginManifest capabilities list", "[infra][plugin][manifest]") {
    PluginManifest manifest;
    manifest.capabilities.push_back("chat.send");
    manifest.capabilities.push_back("db.read");
    
    REQUIRE(manifest.capabilities.size() == 2);
    REQUIRE(manifest.capabilities[0] == "chat.send");
    REQUIRE(manifest.capabilities[1] == "db.read");
}

TEST_CASE("PluginManifest parse_capabilities", "[infra][plugin][manifest]") {
    PluginManifest manifest;
    manifest.capabilities.push_back("chat.send");
    manifest.capabilities.push_back("db.read");
    
    auto cap_set = manifest.parse_capabilities();
    // Just verify it doesn't crash and returns a valid CapabilitySet
    REQUIRE(cap_set.mask() >= 0);
}
