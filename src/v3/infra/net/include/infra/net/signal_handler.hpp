// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file signal_handler.hpp
/// POSIX signal handlers wired to application callbacks.
///
/// Maps system signals to application operations:
///   - SIGHUP  → reload configuration via ConfigWatcher
///   - SIGUSR1 → flush all repositories (save all)
///   - SIGUSR2 → dump metrics to log
///   - SIGINT  → graceful shutdown
///   - SIGTERM → graceful shutdown

#include <memory>
#include <functional>

// Forward declarations
namespace pvpgn::infra::net {
class IoRuntime;
}
namespace pvpgn::infra::config {
class ConfigWatcher;
}
namespace pvpgn::application::ports {
class IUnitOfWorkFactory;
class IMetricsRegistry;
}

namespace pvpgn::infra::net {

/// Installs POSIX signal handlers wired to application callbacks.
class SignalHandler {
public:
    /// Install signal handlers on the given IoRuntime.
    /// @param runtime The IoRuntime instance that owns the io_context
    /// @param config_watcher Handles SIGHUP reloads
    /// @param uow_factory Handles SIGUSR1 save-all operations
    /// @param metrics Handles SIGUSR2 metrics dumps
    /// @param on_shutdown Callback invoked on SIGINT/SIGTERM
    SignalHandler(IoRuntime& runtime,
                  std::shared_ptr<infra::config::ConfigWatcher> config_watcher,
                  std::shared_ptr<application::ports::IUnitOfWorkFactory> uow_factory,
                  std::shared_ptr<application::ports::IMetricsRegistry> metrics,
                  std::function<void(int)> on_shutdown = {});

    ~SignalHandler();

    /// Install the signal handlers.
    /// After calling this, the runtime will route signals to registered handlers.
    void install();

private:
    IoRuntime& runtime_;
    std::shared_ptr<infra::config::ConfigWatcher> config_watcher_;
    std::shared_ptr<application::ports::IUnitOfWorkFactory> uow_factory_;
    std::shared_ptr<application::ports::IMetricsRegistry> metrics_;
    std::function<void(int)> on_shutdown_;

    void on_sighup();
    void on_sigusr1_save_all();
    void on_sigusr2_dump_metrics();
    void on_shutdown(int signal_number);
};

}  // namespace pvpgn::infra::net
