// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/net/signal_handler.hpp"

#include <csignal>
#include <iostream>

#include "core/metrics.hpp"
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
}  // anonymous namespace

SignalHandler::SignalHandler(
    IoRuntime& runtime,
    std::shared_ptr<infra::config::ConfigWatcher> config_watcher,
    std::shared_ptr<application::ports::IUnitOfWorkFactory> uow_factory,
    std::shared_ptr<core::IMetricsRegistry> metrics,
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
    // Register signal handlers via IoRuntime's asio::signal_set.
    // SIGHUP/SIGUSR1/SIGUSR2 are POSIX-only and absent on Windows; there only
    // SIGINT/SIGTERM exist (config-reload / flush-on-signal are POSIX-only).
#if defined(_WIN32)
    runtime_.install_signal_handlers({SIGINT, SIGTERM});
#else
    runtime_.install_signal_handlers({SIGHUP, SIGUSR1, SIGUSR2, SIGINT, SIGTERM});
#endif
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
            // The flush only needs transaction control — depend on the
            // segregated ITransaction sub-interface, not the full
            // repository-bundle IUnitOfWork.
            application::ports::ITransaction& txn = *uow;
            auto result = txn.begin();
            if (!result.has_value()) {
                std::cerr << "Failed to begin unit of work: " << result.error().message() << "\n";
                return;
            }

            // Commit to flush all repositories
            result = txn.commit();
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
