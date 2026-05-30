// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file migrate_cli.hpp
/// CLI argument parsing for pvpgn-migrate.

#include <optional>
#include <string>
#include <string_view>

namespace pvpgn::app::migrate {

// ---------------------------------------------------------------------------
// CLI options
// ---------------------------------------------------------------------------

struct CliOptions {
    std::optional<std::string> from_plain;
    std::optional<std::string> to_sqlite;
    std::optional<std::string> to_toml_file;
    // config subcommand
    bool config_subcommand = false;
    std::optional<std::string> from_conf;
    std::optional<std::string> to_toml;
    bool help = false;
};

// ---------------------------------------------------------------------------
// Functions
// ---------------------------------------------------------------------------

/// Print usage information to stdout.
void print_usage(std::string_view prog);

/// Parse command-line arguments into a CliOptions struct.
/// Returns nullopt on parse error (error message already printed to stderr).
std::optional<CliOptions> parse_args(int argc, char* argv[]);

}  // namespace pvpgn::app::migrate
