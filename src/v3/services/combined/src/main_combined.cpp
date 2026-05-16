// Single-binary entry point for PvPGN
// Runs bnetd + d2cs + d2dbs in a single process.
// Build with: cmake -DPVPGN_SINGLE_BINARY=ON

#include "services/combined/combined_composition.hpp"
#include "runtime/service_host.hpp"
#include <iostream>
#include <csignal>
#include <atomic>
#include <thread>

namespace {
    std::atomic<bool> g_shutdown{false};
}

void signal_handler(int sig) {
    g_shutdown.store(true);
}

int main(int argc, char** argv) {
    // Handle SIGINT/SIGTERM for graceful shutdown
    std::signal(SIGINT,  signal_handler);
    std::signal(SIGTERM, signal_handler);

    pvpgn::runtime::ServiceConfig config;
    config.service_name = "pvpgn-combined";
    // TODO: parse argv for config file path

    pvpgn::services::combined::CombinedComposition composition(config);

    auto init_result = composition.init(config);
    if (!init_result.has_value()) {
        std::cerr << "[pvpgn] Failed to initialize: "
                  << init_result.error() << "\n";
        return 1;
    }

    auto start_result = composition.start();
    if (!start_result.has_value()) {
        std::cerr << "[pvpgn] Failed to start: "
                  << start_result.error() << "\n";
        return 1;
    }

    std::cout << "[pvpgn] All services started. Press Ctrl+C to stop.\n";

    // Wait for shutdown signal
    while (!g_shutdown.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "[pvpgn] Shutting down...\n";
    composition.stop();

    std::cout << "[pvpgn] Shutdown complete.\n";
    return 0;
}
