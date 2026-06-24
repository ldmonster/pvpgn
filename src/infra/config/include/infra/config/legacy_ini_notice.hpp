// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file legacy_ini_notice.hpp
/// Deprecation notice for legacy INI-format configuration parser.
///
/// The INI-format bnetd.conf parser is deprecated as of PvPGN 4.0.
/// All new code should use the TOML-format configuration system.

#if defined(__GNUC__) || defined(__clang__)
    #pragma GCC warning "INI-format configuration parser is deprecated in PvPGN 4.0. Use TOML format instead. See refactoring-plan-15-migration-roadmap.md for migration instructions."
#elif defined(_MSC_VER)
    #pragma message("WARNING: INI-format configuration parser is deprecated in PvPGN 4.0. Use TOML format instead. See refactoring-plan-15-migration-roadmap.md for migration instructions.")
#endif

namespace pvpgn::infra::config {

/// @brief Deprecation marker for INI parser usage.
///
/// Including this header in any translation unit that uses the legacy INI
/// parser will emit a compiler warning. This is intentional and serves as
/// a reminder to migrate to the TOML-based configuration system.
///
/// ## Migration Path
///
/// 1. Use the `pvpgn-conf-convert` tool to convert your existing bnetd.conf:
///    ```bash
///    pvpgn-conf-convert --input bnetd.conf --output bnetd.toml
///    ```
///
/// 2. Review the generated TOML file for correctness.
///
/// 3. Update your application to load the TOML configuration:
///    ```cpp
///    auto config = ServerConfig::from_toml_file("bnetd.toml");
///    ```
///
/// 4. Remove any code that uses the legacy INI parser.
///
/// ## Rationale
///
/// - TOML is more expressive and type-safe than INI
/// - TOML supports nested structures and arrays
/// - TOML has better error reporting
/// - INI parser maintenance burden is high relative to usage
/// - PvPGN 4.0 focuses on TOML-only support
///
/// ## Questions?
///
/// Refer to:
/// - `src/v3/infra/config/include/infra/config/server_config.hpp` — TOML API

}  // namespace pvpgn::infra::config
