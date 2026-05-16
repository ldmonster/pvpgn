// SPDX-License-Identifier: GPL-2.0-or-later
#include "runtime/service_host.hpp"

#include <iostream>
#include <csignal>
#include <cstdlib>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#endif

namespace pvpgn::runtime {

// Global service host instance for signal handling
static ServiceHost* g_service_host = nullptr;

ServiceHost::ServiceHost()
    : composition_(nullptr)
{
}

ServiceHost::~ServiceHost()
{
    if (composition_) {
        composition_->shutdown();
    }
}

int ServiceHost::run(std::unique_ptr<IServiceComposition> composition, int argc, char** argv)
{
    composition_ = std::move(composition);
    
    // Parse command-line arguments
    auto config_result = parse_args(argc, argv);
    if (!config_result.has_value()) {
        std::cerr << "Error parsing arguments: " << config_result.error() << std::endl;
        return 1;
    }
    
    ServiceConfig config = std::move(config_result).value();
    
    // Load configuration file
    auto loaded_config = load_config(config.config_file);
    if (!loaded_config.has_value()) {
        std::cerr << "Error loading config: " << loaded_config.error() << std::endl;
        return 1;
    }
    
    // Merge loaded config with command-line overrides
    config = std::move(loaded_config).value();
    
    // Setup signal handlers
    setup_signals();
    
    // Initialize service
    auto init_result = composition_->init(config);
    if (!init_result.has_value()) {
        std::cerr << "Error initializing service: " << init_result.error() << std::endl;
        return 1;
    }
    
    // Start service
    auto start_result = composition_->start();
    if (!start_result.has_value()) {
        std::cerr << "Error starting service: " << start_result.error() << std::endl;
        return 1;
    }
    
    std::cout << "Service '" << composition_->service_name() << "' started successfully" << std::endl;
    std::cout << "Status: " << composition_->status() << std::endl;
    
    // Keep service running until stopped
    // In a real implementation, this would be an event loop
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    return 0;
}

Result<ServiceConfig, std::string> ServiceHost::parse_args(int argc, char** argv)
{
    ServiceConfig config;
    
    // Simple argument parsing (would use CLI11 in production)
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-c" || arg == "--config") {
            if (i + 1 < argc) {
                config.config_file = argv[++i];
            } else {
                return Result<ServiceConfig, std::string>(
                    core::fail(std::string("Missing value for ") + arg)
                );
            }
        } else if (arg == "-f" || arg == "--foreground") {
            config.foreground = true;
        } else if (arg == "-l" || arg == "--log-level") {
            if (i + 1 < argc) {
                config.log_level = argv[++i];
            } else {
                return Result<ServiceConfig, std::string>(
                    core::fail(std::string("Missing value for ") + arg)
                );
            }
        } else if (arg == "--log-file") {
            if (i + 1 < argc) {
                config.log_file = argv[++i];
            } else {
                return Result<ServiceConfig, std::string>(
                    core::fail(std::string("Missing value for ") + arg)
                );
            }
        } else if (arg == "--pid-file") {
            if (i + 1 < argc) {
                config.pid_file = argv[++i];
            } else {
                return Result<ServiceConfig, std::string>(
                    core::fail(std::string("Missing value for ") + arg)
                );
            }
        } else if (arg == "-w" || arg == "--work-dir") {
            if (i + 1 < argc) {
                config.work_dir = argv[++i];
            } else {
                return Result<ServiceConfig, std::string>(
                    core::fail(std::string("Missing value for ") + arg)
                );
            }
        } else if (arg == "-u" || arg == "--user") {
            if (i + 1 < argc) {
                config.run_as_user = argv[++i];
            } else {
                return Result<ServiceConfig, std::string>(
                    core::fail(std::string("Missing value for ") + arg)
                );
            }
        } else if (arg == "-g" || arg == "--group") {
            if (i + 1 < argc) {
                config.run_as_group = argv[++i];
            } else {
                return Result<ServiceConfig, std::string>(
                    core::fail(std::string("Missing value for ") + arg)
                );
            }
        } else if (arg == "-h" || arg == "--help") {
            std::cout << "Usage: " << argv[0] << " [options]\n"
                      << "Options:\n"
                      << "  -c, --config FILE       Configuration file path\n"
                      << "  -f, --foreground        Run in foreground (don't daemonize)\n"
                      << "  -l, --log-level LEVEL   Log level (trace, debug, info, warn, error, critical)\n"
                      << "  --log-file FILE         Log file path\n"
                      << "  --pid-file FILE         PID file path\n"
                      << "  -w, --work-dir DIR      Working directory\n"
                      << "  -u, --user USER         User to run as (Unix only)\n"
                      << "  -g, --group GROUP       Group to run as (Unix only)\n"
                      << "  -h, --help              Show this help message\n";
            std::exit(0);
        }
    }
    
    return Result<ServiceConfig, std::string>(config);
}

Result<ServiceConfig, std::string> ServiceHost::load_config(const std::string& config_file)
{
    // TODO: Implement actual config file loading
    // For now, return the default config
    ServiceConfig config;
    config.config_file = config_file;
    return Result<ServiceConfig, std::string>(config);
}

void ServiceHost::setup_signals()
{
    g_service_host = this;
    
#ifdef _WIN32
    // Windows signal handling would go here
#else
    // Unix signal handling
    std::signal(SIGTERM, [](int sig) {
        if (g_service_host) {
            g_service_host->handle_signal(sig);
        }
    });
    
    std::signal(SIGINT, [](int sig) {
        if (g_service_host) {
            g_service_host->handle_signal(sig);
        }
    });
    
    std::signal(SIGHUP, [](int sig) {
        if (g_service_host) {
            g_service_host->handle_signal(sig);
        }
    });
#endif
}

void ServiceHost::handle_signal(int sig)
{
    switch (sig) {
        case SIGTERM:
        case SIGINT:
            std::cout << "\nReceived signal " << sig << ", shutting down..." << std::endl;
            if (composition_) {
                composition_->stop();
            }
            std::exit(0);
            break;
#ifndef _WIN32
        case SIGHUP:
            std::cout << "Received SIGHUP, reloading configuration..." << std::endl;
            // TODO: Implement configuration reload
            break;
#endif
        default:
            break;
    }
}

} // namespace pvpgn::runtime
