// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "semver.hpp"
#include <string>
#include <vector>
#include <optional>
#include "core/result.hpp"
#include "core/error.hpp"

namespace pvpgn::scripting::plugin {

/// A single dependency declaration in a plugin manifest
struct PluginDependency {
    std::string plugin_id;          ///< e.g. "pvpgn.chat"
    std::string version_req;        ///< e.g. ">=1.0.0", "^2.1.0"
    bool optional{false};           ///< If true, plugin loads even if dep is missing
};

/// Parsed plugin manifest (from plugin.toml)
struct PluginManifest {
    std::string id;                 ///< Unique plugin ID, e.g. "com.example.quiz"
    std::string name;               ///< Human-readable name
    SemVer version;                 ///< Plugin version
    std::string description;
    std::string author;
    std::string license;
    std::string entry_point;        ///< e.g. "main.lua"
    std::string api_version_req;    ///< Required pvpgn API version, e.g. ">=3.0.0"
    std::vector<PluginDependency> dependencies;
    std::vector<std::string> provides;  ///< Capability tags this plugin provides
    std::vector<std::string> conflicts; ///< Plugin IDs this plugin conflicts with

    /// Parse a plugin.toml file content (TOML format).
    /// Uses a simple hand-rolled TOML parser for the subset we need.
    [[nodiscard]] static core::Result<PluginManifest, core::Error>
    parse_toml(std::string_view toml_content);

    /// Serialize to TOML string
    [[nodiscard]] std::string to_toml() const;
};

} // namespace pvpgn::scripting::plugin
