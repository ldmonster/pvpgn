// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "plugin_manifest.hpp"
#include "semver.hpp"
#include <vector>
#include <string>
#include <unordered_map>
#include "core/result.hpp"
#include "core/error.hpp"

namespace pvpgn::scripting::plugin {

/// Resolution result for a single plugin
struct ResolvedPlugin {
    std::string plugin_id;
    SemVer resolved_version;
    std::vector<std::string> load_order_deps; ///< IDs of plugins that must load first
};

/// DependencyResolver — resolves plugin load order using topological sort.
///
/// Algorithm:
///   1. Build a dependency graph from all manifests
///   2. Check for version requirement satisfaction
///   3. Detect cycles (circular dependencies)
///   4. Topological sort (Kahn's algorithm) to produce load order
///   5. Check for conflicts
class DependencyResolver {
public:
    DependencyResolver() = default;

    /// Register a plugin manifest for resolution.
    void add_plugin(PluginManifest manifest);

    /// Resolve all registered plugins into a load order.
    /// Returns Error if:
    ///   - A required dependency is missing
    ///   - A version requirement is not satisfied
    ///   - A circular dependency is detected
    ///   - Conflicting plugins are both present
    [[nodiscard]] core::Result<std::vector<ResolvedPlugin>, core::Error>
    resolve();

    /// Check if a specific plugin's dependencies are satisfied.
    [[nodiscard]] core::Result<void, core::Error>
    check_plugin(const std::string& plugin_id) const;

    /// Get all registered plugin IDs
    [[nodiscard]] std::vector<std::string> registered_plugins() const;

private:
    std::unordered_map<std::string, PluginManifest> plugins_;

    /// Kahn's algorithm for topological sort
    [[nodiscard]] core::Result<std::vector<std::string>, core::Error>
    topological_sort() const;

    /// Check for conflicts between loaded plugins
    [[nodiscard]] core::Result<void, core::Error>
    check_conflicts() const;
};

} // namespace pvpgn::scripting::plugin
