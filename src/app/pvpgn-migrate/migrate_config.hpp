// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file migrate_config.hpp
/// Legacy *.conf → bnetd.toml migration for pvpgn-migrate.
///
/// Provides:
///   - migrate_conf_to_toml()  — merge legacy *.conf files into a single bnetd.toml

#include <string>

namespace pvpgn::app::migrate {

/// Read legacy *.conf files from @p conf_dir and emit a merged bnetd.toml at
/// @p out_path.
///
/// Reads: bnetd.conf, channel.conf, realm.conf, autoupdate.conf,
///        bnban.conf, bnmotd.txt (if present).
///
/// @return 0 on success, 1 on fatal error.
int migrate_conf_to_toml(const std::string& conf_dir,
                          const std::string& out_path);

}  // namespace pvpgn::app::migrate
