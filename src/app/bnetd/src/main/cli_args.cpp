// SPDX-License-Identifier: GPL-2.0-or-later
// main/cli_args.cpp — Command-line argument parser implementation.

#include "cli_args.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace pvpgn::app::bnetd {

CliArgs parse_args(int argc, char* argv[]) {
    CliArgs args;
    for (int i = 1; i < argc; ++i) {
        std::string_view arg{argv[i]};
        auto next = [&]() -> std::string_view {
            if (i + 1 < argc) return argv[++i];
            throw std::runtime_error(std::string("missing value for ") +
                                     std::string(arg));
        };

        if (arg == "--config" || arg == "-c") {
            args.config_path = std::string(next());
        } else if (arg == "--port" || arg == "-p") {
            args.bnet_port = static_cast<std::uint16_t>(
                std::stoul(std::string(next())));
        } else if (arg == "--wol-port") {
            args.wol_port = static_cast<std::uint16_t>(
                std::stoul(std::string(next())));
        } else if (arg == "--irc-port") {
            args.irc_port = static_cast<std::uint16_t>(
                std::stoul(std::string(next())));
        } else if (arg == "--data-dir" || arg == "-d") {
            args.data_dir = std::string(next());
        } else if (arg == "--log-level" || arg == "-l") {
            args.log_level = std::string(next());
        } else if (arg == "--threads" || arg == "-t") {
            args.threads = static_cast<std::uint32_t>(
                std::stoul(std::string(next())));
        } else if (arg == "--help" || arg == "-h") {
            std::cout <<
                "Usage: pvpgn_v3_bnetd [options]\n"
                "  --config,    -c <path>   Path to bnetd.toml\n"
                "  --port,      -p <port>   BNet/BNFTP port (default 6112)\n"
                "  --wol-port      <port>   Westwood Online port (default 4000)\n"
                "  --irc-port      <port>   IRC bridge port (default 6667)\n"
                "  --data-dir,  -d <path>   Data directory (default .)\n"
                "  --log-level, -l <level>  Log level (default info)\n"
                "  --threads,   -t <n>      Worker threads (default hw_concurrency)\n"
                "  --version,   -V          Print version and exit\n"
                "  --help,      -h          Show this help\n";
            std::exit(0);
        } else if (arg == "--version" || arg == "-V") {
#ifndef PVPGN_VERSION
#  define PVPGN_VERSION "unknown"
#endif
            std::cout << "pvpgn_v3_bnetd " << PVPGN_VERSION << "\n";
            std::exit(0);
        }
    }
    return args;
}

} // namespace pvpgn::app::bnetd
