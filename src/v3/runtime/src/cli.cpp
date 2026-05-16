// SPDX-License-Identifier: GPL-2.0-or-later
/// CLI argument parsing utilities for service host
/// Provides dependency-free command-line argument parsing

#include "runtime/service_host.hpp"
#include <iostream>
#include <sstream>

namespace pvpgn::runtime {

/// Parse command-line arguments
/// This is a helper function that can be used by services to parse their arguments
class CliParser {
public:
    /// Parse arguments and return ServiceConfig
    /// Supports both short and long options with optional values
    static Result<ServiceConfig, std::string> parse(int argc, char** argv)
    {
        ServiceConfig config;
        
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            
            if (arg == "-c" || arg == "--config") {
                if (i + 1 < argc) {
                    config.config_file = argv[++i];
                } else {
                    return Result<ServiceConfig, std::string>(
                        core::fail(std::string("Option ") + arg + " requires a value")
                    );
                }
            } else if (arg == "-f" || arg == "--foreground") {
                config.foreground = true;
            } else if (arg == "-l" || arg == "--log-level") {
                if (i + 1 < argc) {
                    config.log_level = argv[++i];
                } else {
                    return Result<ServiceConfig, std::string>(
                        core::fail(std::string("Option ") + arg + " requires a value")
                    );
                }
            } else if (arg == "--log-file") {
                if (i + 1 < argc) {
                    config.log_file = argv[++i];
                } else {
                    return Result<ServiceConfig, std::string>(
                        core::fail(std::string("Option ") + arg + " requires a value")
                    );
                }
            } else if (arg == "--pid-file") {
                if (i + 1 < argc) {
                    config.pid_file = argv[++i];
                } else {
                    return Result<ServiceConfig, std::string>(
                        core::fail(std::string("Option ") + arg + " requires a value")
                    );
                }
            } else if (arg == "-w" || arg == "--work-dir") {
                if (i + 1 < argc) {
                    config.work_dir = argv[++i];
                } else {
                    return Result<ServiceConfig, std::string>(
                        core::fail(std::string("Option ") + arg + " requires a value")
                    );
                }
            } else if (arg == "-u" || arg == "--user") {
                if (i + 1 < argc) {
                    config.run_as_user = argv[++i];
                } else {
                    return Result<ServiceConfig, std::string>(
                        core::fail(std::string("Option ") + arg + " requires a value")
                    );
                }
            } else if (arg == "-g" || arg == "--group") {
                if (i + 1 < argc) {
                    config.run_as_group = argv[++i];
                } else {
                    return Result<ServiceConfig, std::string>(
                        core::fail(std::string("Option ") + arg + " requires a value")
                    );
                }
            } else if (arg == "-h" || arg == "--help") {
                print_help(argv[0]);
                std::exit(0);
            } else if (arg == "-v" || arg == "--version") {
                std::cout << "PvPGN Service Host v3.0.0\n";
                std::exit(0);
            } else {
                return Result<ServiceConfig, std::string>(
                    core::fail(std::string("Unknown option: ") + arg)
                );
            }
        }
        
        return Result<ServiceConfig, std::string>(config);
    }
    
private:
    static void print_help(const char* program_name)
    {
        std::cout << "Usage: " << program_name << " [options]\n\n"
                  << "Options:\n"
                  << "  -c, --config FILE       Configuration file path\n"
                  << "  -f, --foreground        Run in foreground (don't daemonize)\n"
                  << "  -l, --log-level LEVEL   Log level (trace, debug, info, warn, error, critical)\n"
                  << "  --log-file FILE         Log file path (empty = stdout)\n"
                  << "  --pid-file FILE         PID file path\n"
                  << "  -w, --work-dir DIR      Working directory (default: .)\n"
                  << "  -u, --user USER         User to run as (Unix only)\n"
                  << "  -g, --group GROUP       Group to run as (Unix only)\n"
                  << "  -h, --help              Show this help message\n"
                  << "  -v, --version           Show version information\n";
    }
};

} // namespace pvpgn::runtime
