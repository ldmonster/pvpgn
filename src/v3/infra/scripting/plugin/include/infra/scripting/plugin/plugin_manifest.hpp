#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include "capability.hpp"

namespace pvpgn::infra::scripting {

/// Plugin manifest loaded from plugin.toml
struct PluginManifest {
    /// Plugin name (e.g., "tournament-pack")
    std::string name;
    
    /// Semantic version (e.g., "1.2.0")
    std::string version;
    
    /// Plugin authors
    std::vector<std::string> authors;
    
    /// API version this plugin targets (e.g., "3.0")
    std::string api_version;
    
    /// Entry point: "main.lua" for Lua, "lib.so" for native
    std::string entry;
    
    /// Plugin type: "lua" or "native"
    std::string plugin_type;
    
    /// Required capabilities
    std::vector<std::string> capabilities;
    
    /// Human-readable description
    std::string description;
    
    /// Plugin directory path
    std::string plugin_dir;
    
    /// Parse capabilities into a CapabilitySet
    CapabilitySet parse_capabilities() const;
};

/// Load and parse plugin.toml from a directory
/// Returns error message if parsing fails
std::string load_plugin_manifest(const std::string& plugin_dir, PluginManifest& out);

} // namespace pvpgn::infra::scripting
