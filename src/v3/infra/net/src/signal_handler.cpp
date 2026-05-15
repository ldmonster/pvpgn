// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/net/signal_handler.hpp"

#include <csignal>
#include <iostream>

#include "application/ports/metrics_registry.hpp"
#include "application/ports/unit_of_work_factory.hpp"
#include "core/logging.hpp"
#include "infra/config/config_watcher.hpp"
#include "infra/net/io_runtime.hpp"

namespace pvpgn::infra::net {

namespace {
// Global handler instance for static signal handler functions
// This is intentional - signal handlers require C linkage and static storage
SignalHandler* g_signal_handler = nullptr;

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
    SPDLOG_INFO("Received SIGHUP, reloading configuration");
    if (config_watcher_) {
        auto result = config_watcher_->reload();
        if (result.has_value()) {
            SPDLOG_INFO("Configuration reloaded successfully");
        } else {
            SPDLOG_ERROR("Failed to reload configuration: {}", result.error().message());
        }
    }
}

void SignalHandler::on_sigusr1_save_all() {
    SPDLOG_INFO("Received SIGUSR1, flushing all repositories");
    if (uow_factory_) {
        try {
            auto uow = uow_factory_->create();
            auto result = uow->begin();
            if (!result.has_value()) {
                SPDLOG_ERROR("Failed to begin unit of work: {}", result.error().message());
                return;
            }

            // Commit to flush all repositories
            result = uow->commit();
            if (result.has_value()) {
                SPDLOG_INFO("All repositories flushed successfully");
            } else {
                SPDLOG_ERROR("Failed to commit unit of work: {}", result.error().message());
            }
        } catch (const std::exception& e) {
            SPDLOG_ERROR("Exception during save-all: {}", e.what());
        }
    }
}

void SignalHandler::on_sigusr2_dump_metrics() {
    SPDLOG_INFO("Received SIGUSR2, dumping metrics");
    if (metrics_) {
        std::string metrics_output = metrics_->serialize();
        SPDLOG_INFO("=== METRICS DUMP ===");
        SPDLOG_INFO("{}", metrics_output);
        SPDLOG_INFO("=== END METRICS DUMP ===");
    }
}

void SignalHandler::on_shutdown(int signal_number) {
    SPDLOG_INFO("Received signal {}, initiating graceful shutdown", signal_number);
    if (on_shutdown_) {
        on_shutdown_(signal_number);
    }
    // The runtime itself should stop via asio::signal_set mechanism
    runtime_.stop();
}

}  // namespace pvpgn::infra::net
