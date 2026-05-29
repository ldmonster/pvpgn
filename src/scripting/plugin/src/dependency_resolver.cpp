// SPDX-License-Identifier: GPL-2.0-or-later
#include "scripting/plugin/dependency_resolver.hpp"
#include <unordered_set>
#include <queue>
#include <algorithm>

namespace pvpgn::scripting::plugin {

void DependencyResolver::add_plugin(PluginManifest manifest) {
    plugins_[manifest.id] = std::move(manifest);
}

std::vector<std::string> DependencyResolver::registered_plugins() const {
    std::vector<std::string> result;
    for (const auto& [id, _] : plugins_) {
        result.push_back(id);
    }
    return result;
}

core::Result<void, core::Error> DependencyResolver::check_plugin(const std::string& plugin_id) const {
    auto it = plugins_.find(plugin_id);
    if (it == plugins_.end()) {
        return core::fail(core::Error(core::StatusCode::NotFound,
            "Plugin not found: " + plugin_id));
    }

    const auto& manifest = it->second;

    // Check all required dependencies
    for (const auto& dep : manifest.dependencies) {
        if (dep.optional) {
            continue; // Skip optional dependencies
        }

        auto dep_it = plugins_.find(dep.plugin_id);
        if (dep_it == plugins_.end()) {
            return core::fail(core::Error(core::StatusCode::NotFound,
                "Required dependency not found: " + dep.plugin_id + " (required by " + plugin_id + ")"));
        }

        // Check version requirement
        if (!dep_it->second.version.satisfies(dep.version_req)) {
            return core::fail(core::Error(core::StatusCode::FailedPrecondition,
                "Version requirement not satisfied: " + dep.plugin_id + " " + dep.version_req +
                " (got " + dep_it->second.version.to_string() + ")"));
        }
    }

    return core::Result<void, core::Error>();
}

core::Result<std::vector<std::string>, core::Error> DependencyResolver::topological_sort() const {
    // Build adjacency list and in-degree map
    std::unordered_map<std::string, std::vector<std::string>> adj;
    std::unordered_map<std::string, size_t> in_degree;

    // Initialize all nodes
    for (const auto& [id, _] : plugins_) {
        adj[id] = {};
        in_degree[id] = 0;
    }

    // Build edges: for each plugin, add edges from its dependencies to itself
    for (const auto& [id, manifest] : plugins_) {
        for (const auto& dep : manifest.dependencies) {
            auto dep_it = plugins_.find(dep.plugin_id);
            if (dep_it != plugins_.end()) {
                // Edge from dependency to dependent
                adj[dep.plugin_id].push_back(id);
                in_degree[id]++;
            }
        }
    }

    // Kahn's algorithm
    std::queue<std::string> queue;
    for (const auto& [id, degree] : in_degree) {
        if (degree == 0) {
            queue.push(id);
        }
    }

    std::vector<std::string> result;
    while (!queue.empty()) {
        std::string node = queue.front();
        queue.pop();
        result.push_back(node);

        for (const auto& neighbor : adj[node]) {
            in_degree[neighbor]--;
            if (in_degree[neighbor] == 0) {
                queue.push(neighbor);
            }
        }
    }

    // Check for cycles
    if (result.size() != plugins_.size()) {
        return core::fail(core::Error(core::StatusCode::Aborted,
            "Circular dependency detected"));
    }

    return result;
}

core::Result<void, core::Error> DependencyResolver::check_conflicts() const {
    for (const auto& [id, manifest] : plugins_) {
        for (const auto& conflict_id : manifest.conflicts) {
            if (plugins_.find(conflict_id) != plugins_.end()) {
                return core::fail(core::Error(core::StatusCode::FailedPrecondition,
                    "Plugin conflict: " + id + " conflicts with " + conflict_id));
            }
        }
    }
    return core::Result<void, core::Error>();
}

core::Result<std::vector<ResolvedPlugin>, core::Error> DependencyResolver::resolve() {
    // Check for conflicts first
    auto conflict_check = check_conflicts();
    if (!conflict_check.has_value()) {
        return core::fail(std::move(conflict_check).error());
    }

    // Check all plugins' dependencies
    for (const auto& [id, _] : plugins_) {
        auto check = check_plugin(id);
        if (!check.has_value()) {
            return core::fail(std::move(check).error());
        }
    }

    // Perform topological sort
    auto sort_result = topological_sort();
    if (!sort_result.has_value()) {
        return core::fail(std::move(sort_result).error());
    }

    auto sorted_ids = std::move(sort_result).value();

    // Build result with dependency information
    std::vector<ResolvedPlugin> result;
    for (const auto& id : sorted_ids) {
        const auto& manifest = plugins_.at(id);
        ResolvedPlugin resolved;
        resolved.plugin_id = id;
        resolved.resolved_version = manifest.version;

        // Collect direct dependencies that are in the resolved set
        for (const auto& dep : manifest.dependencies) {
            if (plugins_.find(dep.plugin_id) != plugins_.end()) {
                resolved.load_order_deps.push_back(dep.plugin_id);
            }
        }

        result.push_back(std::move(resolved));
    }

    return result;
}

} // namespace pvpgn::scripting::plugin
