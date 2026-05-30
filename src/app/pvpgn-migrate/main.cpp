// SPDX-License-Identifier: GPL-2.0-or-later

/// @file main.cpp
/// Entry point for the `pvpgn-migrate` CLI tool.
///
/// Supported subcommands
/// ---------------------
///   config --from-conf <dir> --to <bnetd.toml>
///       Read legacy *.conf files from <dir> and emit a merged bnetd.toml.
///       Reads: bnetd.conf, channel.conf, realm.conf, autoupdate.conf,
///              bnban.conf, bnmotd.txt (if present).
///       Produces a TOML file that bnetd --check-config accepts.
///
///   --from-plain <dir> --to-sqlite <db_path>
///       Migrate all *.plain account files from <dir> into a SQLite database
///       at <db_path>.  The database is created if it does not exist and all
///       schema migrations are applied before any data is written.
///
///   --from-plain <dir> --to-toml-file <dst_dir>
///       Convert all *.plain account files from <dir> into TOML files written
///       to <dst_dir>.  Each output file is named <username>.toml.
///
/// Exit codes
/// ----------
///   0  — success (all accounts processed, even if some were skipped)
///   1  — fatal error (bad arguments, source directory not found, …)

#include "migrate_accounts.hpp"
#include "migrate_cli.hpp"
#include "migrate_config.hpp"

#include <iostream>

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main(int argc, char* argv[]) {
    const std::string_view prog = (argc > 0) ? argv[0] : "pvpgn-migrate";

    if (argc < 2) {
        pvpgn::app::migrate::print_usage(prog);
        return 1;
    }

    auto opts_opt = pvpgn::app::migrate::parse_args(argc, argv);
    if (!opts_opt) {
        pvpgn::app::migrate::print_usage(prog);
        return 1;
    }
    const auto& opts = *opts_opt;

    if (opts.help) {
        pvpgn::app::migrate::print_usage(prog);
        return 0;
    }

    // Dispatch: config subcommand
    if (opts.config_subcommand) {
        if (!opts.from_conf) {
            std::cerr << "error: config subcommand requires --from-conf <dir>\n";
            pvpgn::app::migrate::print_usage(prog);
            return 1;
        }
        if (!opts.to_toml) {
            std::cerr << "error: config subcommand requires --to <bnetd.toml>\n";
            pvpgn::app::migrate::print_usage(prog);
            return 1;
        }
        return pvpgn::app::migrate::migrate_conf_to_toml(*opts.from_conf, *opts.to_toml);
    }

    // Validate: --from-plain is always required for account migration
    if (!opts.from_plain) {
        std::cerr << "error: --from-plain <dir> is required\n";
        pvpgn::app::migrate::print_usage(prog);
        return 1;
    }

    // Validate: exactly one destination must be specified
    const bool has_sqlite    = opts.to_sqlite.has_value();
    const bool has_toml_file = opts.to_toml_file.has_value();

    if (!has_sqlite && !has_toml_file) {
        std::cerr << "error: specify --to-sqlite or --to-toml-file\n";
        pvpgn::app::migrate::print_usage(prog);
        return 1;
    }
    if (has_sqlite && has_toml_file) {
        std::cerr << "error: --to-sqlite and --to-toml-file are mutually exclusive\n";
        pvpgn::app::migrate::print_usage(prog);
        return 1;
    }

    // Dispatch
    if (has_sqlite) {
        return pvpgn::app::migrate::migrate_plain_to_sqlite(*opts.from_plain, *opts.to_sqlite);
    }
    return pvpgn::app::migrate::migrate_plain_to_toml(*opts.from_plain, *opts.to_toml_file);
}
