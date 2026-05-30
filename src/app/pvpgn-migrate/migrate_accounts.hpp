// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file migrate_accounts.hpp
/// Account migration functions for pvpgn-migrate.
///
/// Provides:
///   - migrate_plain_to_sqlite()  — *.plain → SQLite database
///   - migrate_plain_to_toml()    — *.plain → per-account TOML files

#include <string>

namespace pvpgn::app::migrate {

/// Migrate all *.plain account files from @p src_dir into a SQLite database
/// at @p db_path.  The database is created if it does not exist and all
/// schema migrations are applied before any data is written.
///
/// @return 0 on success, 1 on fatal error.
int migrate_plain_to_sqlite(const std::string& src_dir,
                             const std::string& db_path);

/// Convert all *.plain account files from @p src_dir into TOML files written
/// to @p dst_dir.  Each output file is named <username>.toml.
///
/// @return 0 on success, 1 on fatal error.
int migrate_plain_to_toml(const std::string& src_dir,
                           const std::string& dst_dir);

}  // namespace pvpgn::app::migrate
