// SPDX-License-Identifier: GPL-2.0-or-later
// main/cli_args.hpp — Command-line argument structure and parser declaration.
#pragma once

#include <cstdint>
#include <string>

namespace pvpgn::app::bnetd {

struct CliArgs {
    std::string   config_path;
    std::uint16_t bnet_port{0};
    std::uint16_t wol_port{0};   ///< 0 = use config/default (4000)
    std::uint16_t irc_port{0};   ///< 0 = use config/default (6667)
    std::string   data_dir;
    std::string   log_level;
    std::uint32_t threads{0};
};

/// Parse argc/argv into a CliArgs struct.
/// Prints help/version and calls std::exit() for --help / --version.
/// Throws std::runtime_error on missing argument values.
CliArgs parse_args(int argc, char* argv[]);

} // namespace pvpgn::app::bnetd
