// SPDX-License-Identifier: GPL-2.0-or-later

/// @file migrate_cli.cpp
/// CLI argument parsing for pvpgn-migrate.

#include "migrate_cli.hpp"

#include <iostream>
#include <vector>

namespace pvpgn::app::migrate {

void print_usage(std::string_view prog) {
    std::cout
        << "Usage:\n"
        << "  " << prog << " config --from-conf <conf_dir> --to <bnetd.toml>\n"
        << "  " << prog << " --from-plain <src_dir> --to-sqlite <db_path>\n"
        << "  " << prog << " --from-plain <src_dir> --to-toml-file <dst_dir>\n"
        << "\n"
        << "Subcommands:\n"
        << "  config                  Merge legacy *.conf files into a single bnetd.toml\n"
        << "\n"
        << "Options:\n"
        << "  --from-conf <dir>       Source directory containing legacy *.conf files\n"
        << "  --to <bnetd.toml>       Output TOML file path\n"
        << "  --from-plain <dir>      Source directory containing *.plain account files\n"
        << "  --to-sqlite  <db_path>  Target SQLite database file (created if absent)\n"
        << "  --to-toml-file <dir>    Target directory for *.toml output files\n"
        << "  --help, -h              Show this help message\n";
}

std::optional<CliOptions> parse_args(int argc, char* argv[]) {
    CliOptions opts;
    const std::vector<std::string_view> args(argv + 1, argv + argc);

    if (args.empty()) return opts;

    std::size_t start = 0;

    // Check for 'config' positional subcommand
    if (args[0] == "config") {
        opts.config_subcommand = true;
        start = 1;
    }

    for (std::size_t i = start; i < args.size(); ++i) {
        const auto arg = args[i];

        if (arg == "--help" || arg == "-h") {
            opts.help = true;
            return opts;
        }

        auto require_next = [&](std::string_view flag) -> std::optional<std::string> {
            if (i + 1 >= args.size()) {
                std::cerr << "error: " << flag << " requires an argument\n";
                return std::nullopt;
            }
            return std::string{args[++i]};
        };

        if (arg == "--from-plain") {
            auto val = require_next(arg);
            if (!val) return std::nullopt;
            opts.from_plain = std::move(val);
        } else if (arg == "--to-sqlite") {
            auto val = require_next(arg);
            if (!val) return std::nullopt;
            opts.to_sqlite = std::move(val);
        } else if (arg == "--to-toml-file") {
            auto val = require_next(arg);
            if (!val) return std::nullopt;
            opts.to_toml_file = std::move(val);
        } else if (arg == "--from-conf") {
            auto val = require_next(arg);
            if (!val) return std::nullopt;
            opts.from_conf = std::move(val);
        } else if (arg == "--to") {
            auto val = require_next(arg);
            if (!val) return std::nullopt;
            opts.to_toml = std::move(val);
        } else {
            std::cerr << "error: unknown option: " << arg << "\n";
            return std::nullopt;
        }
    }

    return opts;
}

}  // namespace pvpgn::app::migrate
