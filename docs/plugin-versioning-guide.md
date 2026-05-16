# Plugin Versioning and Dependency Resolution Guide

## Overview

PvPGN-PRO v3 includes a comprehensive plugin versioning and dependency resolution system based on **Semantic Versioning 2.0** (SemVer) and **topological sorting** for load order determination.

## Semantic Versioning (SemVer 2.0)

All plugin versions follow the SemVer 2.0 specification: `MAJOR.MINOR.PATCH[-prerelease][+build]`

### Format

- **MAJOR**: Increment for incompatible API changes
- **MINOR**: Increment for backwards-compatible functionality additions
- **PATCH**: Increment for backwards-compatible bug fixes
- **prerelease** (optional): Indicates a pre-release version (e.g., `alpha`, `beta`, `rc.1`)
- **build** (optional): Build metadata (ignored in version comparisons)

### Examples

```
1.0.0              # Release version
2.1.3-alpha        # Pre-release version
1.0.0-beta.1       # Pre-release with identifier
1.0.0+build.123    # Build metadata
1.0.0-rc.1+sha.5   # Pre-release with build metadata
```

### Version Precedence

1. Release versions have higher precedence than pre-release versions
2. Pre-release versions are compared identifier by identifier
3. Numeric identifiers are compared numerically
4. Alphanumeric identifiers are compared lexicographically
5. Build metadata is ignored in comparisons

**Example ordering:**
```
1.0.0-alpha < 1.0.0-alpha.1 < 1.0.0-alpha.beta < 1.0.0-beta < 1.0.0-beta.2 < 1.0.0-rc.1 < 1.0.0
```

## Version Requirements

Version requirements specify which versions of a dependency are acceptable. The following operators are supported:

### Exact Match (`=`)

```toml
version_req = "=1.2.3"
```

Matches only version `1.2.3`.

### Not Equal (`!=`)

```toml
version_req = "!=1.2.3"
```

Matches any version except `1.2.3`.

### Comparison Operators

```toml
version_req = ">=1.2.0"   # Greater than or equal
version_req = ">1.2.0"    # Greater than
version_req = "<=2.0.0"   # Less than or equal
version_req = "<2.0.0"    # Less than
```

### Caret (`^`) — Compatible Versions

The caret operator allows changes that do not modify the left-most non-zero digit.

```toml
version_req = "^1.2.3"    # >=1.2.3 <2.0.0
version_req = "^0.2.3"    # >=0.2.3 <0.3.0
version_req = "^0.0.3"    # >=0.0.3 <0.0.4
```

### Tilde (`~`) — Patch-Level Changes

The tilde operator allows patch-level changes only.

```toml
version_req = "~1.2.3"    # >=1.2.3 <1.3.0
version_req = "~1.2"      # >=1.2.0 <1.3.0
```

## Plugin Manifest Format (TOML)

Plugin metadata is defined in a `plugin.toml` file at the root of the plugin directory.

### Required Fields

```toml
[plugin]
id = "com.example.myplugin"
name = "My Plugin"
version = "1.0.0"
entry_point = "main.lua"
```

- **id**: Unique plugin identifier (reverse domain notation recommended)
- **name**: Human-readable plugin name
- **version**: Plugin version (SemVer format)
- **entry_point**: Path to the main Lua script (relative to plugin directory)

### Optional Fields

```toml
[plugin]
description = "A brief description of what the plugin does"
author = "Author Name"
license = "GPL-2.0"
api_version_req = ">=3.0.0"
provides = ["capability.one", "capability.two"]
conflicts = ["com.example.incompatible"]
```

- **description**: Plugin description
- **author**: Plugin author name
- **license**: License identifier (SPDX format recommended)
- **api_version_req**: Required PvPGN API version (version requirement format)
- **provides**: List of capabilities this plugin provides
- **conflicts**: List of plugin IDs that conflict with this plugin

### Dependencies

Dependencies are declared as array sections:

```toml
[[dependencies]]
plugin_id = "com.pvpgn.core.chat"
version_req = ">=3.0.0"
optional = false

[[dependencies]]
plugin_id = "com.pvpgn.core.database"
version_req = "^3.1.0"
optional = true
```

- **plugin_id**: ID of the required plugin
- **version_req**: Version requirement (using operators above)
- **optional**: If `true`, plugin loads even if dependency is missing

## Complete Example

```toml
# plugins/example-quiz/plugin.toml

[plugin]
id = "com.pvpgn.example.quiz"
name = "Quiz Plugin"
version = "1.0.0"
description = "Example quiz game plugin for PvPGN"
author = "PvPGN Team"
license = "GPL-2.0"
entry_point = "main.lua"
api_version_req = ">=3.0.0"
provides = ["game.quiz"]
conflicts = []

[[dependencies]]
plugin_id = "com.pvpgn.core.chat"
version_req = ">=3.0.0"
optional = false

[[dependencies]]
plugin_id = "com.pvpgn.core.database"
version_req = "^3.1.0"
optional = true
```

## Dependency Resolution Algorithm

The dependency resolver uses the following algorithm:

### 1. Validation Phase

- Check that all required (non-optional) dependencies are registered
- Verify that each dependency's version satisfies the version requirement
- Detect plugin conflicts

### 2. Conflict Detection

- For each plugin, check if any of its conflicting plugins are also loaded
- Return error if conflicts are detected

### 3. Topological Sort (Kahn's Algorithm)

- Build a directed acyclic graph (DAG) of dependencies
- Compute in-degree for each node
- Process nodes with in-degree 0, decrementing neighbors' in-degrees
- Detect cycles if the result size doesn't match the total plugin count

### 4. Load Order Generation

- Return plugins in topological order
- Each plugin is guaranteed to load after all its dependencies

## Error Handling

The resolver returns detailed errors for:

- **Missing Dependencies**: Required plugin not found
- **Version Mismatch**: Dependency version doesn't satisfy requirement
- **Circular Dependencies**: Cycle detected in dependency graph
- **Plugin Conflicts**: Conflicting plugins both present
- **Invalid Manifest**: Malformed TOML or missing required fields

All errors are returned as `core::Result<T, core::Error>` with descriptive messages.

## Usage Example (C++)

```cpp
#include "scripting/plugin/dependency_resolver.hpp"
#include "scripting/plugin/plugin_manifest.hpp"

// Parse plugin manifests
auto manifest1 = PluginManifest::parse_toml(toml_content1);
auto manifest2 = PluginManifest::parse_toml(toml_content2);

if (!manifest1.has_value() || !manifest2.has_value()) {
    // Handle parse error
    return;
}

// Create resolver and register plugins
DependencyResolver resolver;
resolver.add_plugin(std::move(manifest1).value());
resolver.add_plugin(std::move(manifest2).value());

// Resolve load order
auto result = resolver.resolve();
if (!result.has_value()) {
    // Handle resolution error
    std::cerr << "Resolution failed: " << result.error().message() << std::endl;
    return;
}

// Load plugins in order
for (const auto& resolved : result.value()) {
    std::cout << "Loading: " << resolved.plugin_id 
              << " v" << resolved.resolved_version.to_string() << std::endl;
    // Load plugin...
}
```

## Best Practices

### Version Numbering

1. Start with `0.1.0` for initial development
2. Use `1.0.0` for the first stable release
3. Increment MAJOR for breaking changes
4. Increment MINOR for new features
5. Increment PATCH for bug fixes

### Dependency Declaration

1. Use caret (`^`) for compatible versions by default
2. Use tilde (`~`) for conservative updates
3. Mark truly optional dependencies with `optional = true`
4. Declare conflicts explicitly to prevent loading incompatible plugins

### Plugin IDs

1. Use reverse domain notation: `com.organization.pluginname`
2. Use lowercase letters and dots only
3. Make IDs globally unique and stable

### Manifest Maintenance

1. Keep descriptions concise and informative
2. Use SPDX license identifiers
3. List all direct dependencies (not transitive)
4. Update version requirements when dependencies change

## Limitations and Future Enhancements

### Current Limitations

- No version pinning (exact match only)
- No pre-release filtering
- No platform-specific dependencies
- No optional feature flags

### Potential Future Enhancements

- Semantic version ranges (e.g., `1.x || >=2.3.5`)
- Pre-release filtering policies
- Platform and architecture constraints
- Feature flags and optional capabilities
- Dependency version locking
- Plugin update checking and auto-update

## References

- [Semantic Versioning 2.0.0](https://semver.org/)
- [TOML Format](https://toml.io/)
- [Topological Sorting](https://en.wikipedia.org/wiki/Topological_sorting)
