// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/net/signal_handler.hpp"

#include <csignal>
#include <iostream>

#include "application/ports/metrics_registry.hpp"
#include "application/persistence/unit_of_work_factory.hpp"
#include "application/persistence/unit_of_work.hpp"
#include "core/logging.hpp"
#include "infra/config/config_watcher.hpp"
#include "infra/net/io_runtime.hpp"

namespace pvpgn::infra::net {

namespace {
// Global handler instance for static signal handler functions
// This is intentional - signal handlers require C linkage and static storage
SignalHandler* g_signal_handler = nullptr;

// These handlers are registered with the signal set but not directly called
// They exist for future use when signal handling is fully integrated
#if 0
void handle_sighup(int) {
    if (g_signal_handler) {
        g_signal_handler->install();  // Re-register handlers
    }
}

void handle_sigusr1(int) {
    if (g_signal_handler) {
        // Handler will be invoked in the post() callback
    }
}

void handle_sigusr2(int) {
    if (g_signal_handler) {
        // Handler will be invoked in the post() callback
    }
}

void handle_sigint_sigterm(int sig) {
    if (g_signal_handler) {
        // Handler will be invoked in the post() callback
    }
}
#endif
}  // anonymous namespace

SignalHandler::SignalHandler(
    IoRuntime& runtime,
    std::shared_ptr<infra::config::ConfigWatcher> config_watcher,
    std::shared_ptr<application::ports::IUnitOfWorkFactory> uow_factory,
    std::shared_ptr<application::ports::IMetricsRegistry> metrics,
    std::function<void(int)> on_shutdown)
    : runtime_(runtime),
      config_watcher_(config_watcher),
      uow_factory_(uow_factory),
      metrics_(metrics),
      on_shutdown_(on_shutdown) {
    g_signal_handler = this;
}

SignalHandler::~SignalHandler() {
    g_signal_handler = nullptr;
}

void SignalHandler::install() {
    // Register signal handlers via IoRuntime's asio::signal_set
    runtime_.install_signal_handlers({SIGHUP, SIGUSR1, SIGUSR2, SIGINT, SIGTERM});
}

void SignalHandler::on_sighup() {
    std::cerr << "Received SIGHUP, reloading configuration\n";
    if (config_watcher_) {
        auto result = config_watcher_->reload();
        if (result.has_value()) {
            std::cerr << "Configuration reloaded successfully\n";
        } else {
            std::cerr << "Failed to reload configuration: " << result.error().message() << "\n";
        }
    }
}

void SignalHandler::on_sigusr1_save_all() {
    std::cerr << "Received SIGUSR1, flushing all repositories\n";
    if (uow_factory_) {
        try {
            auto uow = uow_factory_->create();
            auto result = uow->begin();
            if (!result.has_value()) {
                std::cerr << "Failed to begin unit of work: " << result.error().message() << "\n";
                return;
            }

            // Commit to flush all repositories
            result = uow->commit();
            if (result.has_value()) {
                std::cerr << "All repositories flushed successfully\n";
            } else {
                std::cerr << "Failed to commit unit of work: " << result.error().message() << "\n";
            }
        } catch (const std::exception& e) {
            std::cerr << "Exception during save-all: " << e.what() << "\n";
        }
    }
}

void SignalHandler::on_sigusr2_dump_metrics() {
    std::cerr << "Received SIGUSR2, dumping metrics\n";
    if (metrics_) {
        std::string metrics_output = metrics_->serialize();
        std::cerr << "=== METRICS DUMP ===\n";
        std::cerr << metrics_output << "\n";
        std::cerr << "=== END METRICS DUMP ===\n";
    }
}

void SignalHandler::on_shutdown(int signal_number) {
    std::cerr << "Received signal " << signal_number << ", initiating graceful shutdown\n";
    if (on_shutdown_) {
        on_shutdown_(signal_number);
    }
    // The runtime itself should stop via asio::signal_set mechanism
    runtime_.stop();
}

}  // namespace pvpgn::infra::net
