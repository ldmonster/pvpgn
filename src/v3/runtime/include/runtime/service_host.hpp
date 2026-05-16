#pragma once

#include <memory>
#include <string>
#include <vector>
#include <functional>
#include "core/result.hpp"

using pvpgn::core::Result;

namespace pvpgn::runtime {

/// Service host configuration
struct ServiceConfig {
    /// Service name
    std::string service_name;
    
    /// Config file path
    std::string config_file;
    
    /// Run in foreground (don't daemonize)
    bool foreground = false;
    
    /// Log level (trace, debug, info, warn, error, critical)
    std::string log_level = "info";
    
    /// Log file path (empty = stdout)
    std::string log_file;
    
    /// PID file path
    std::string pid_file;
    
    /// Working directory
    std::string work_dir = ".";
    
    /// User to run as (Unix only)
    std::string run_as_user;
    
    /// Group to run as (Unix only)
    std::string run_as_group;
};

/// Service composition root interface
/// Each service (bnetd, d2cs, d2dbs) implements this to provide its dependencies
class IServiceComposition {
public:
    virtual ~IServiceComposition() = default;
    
    /// Get service name (e.g., "bnetd", "d2cs", "d2dbs")
    virtual std::string service_name() const = 0;
    
    /// Initialize service with config
    virtual Result<void, std::string> init(const ServiceConfig& config) = 0;
    
    /// Start service (begin accepting connections, etc.)
    virtual Result<void, std::string> start() = 0;
    
    /// Stop service gracefully
    virtual void stop() = 0;
    
    /// Shutdown service (cleanup)
    virtual void shutdown() = 0;
    
    /// Get service status
    virtual std::string status() const = 0;
};

/// Main service host
class ServiceHost {
public:
    ServiceHost();
    ~ServiceHost();
    
    /// Run service with given composition
    /// Returns exit code
    int run(std::unique_ptr<IServiceComposition> composition, int argc, char** argv);
    
private:
    std::unique_ptr<IServiceComposition> composition_;
    
    Result<ServiceConfig, std::string> parse_args(int argc, char** argv);
    Result<ServiceConfig, std::string> load_config(const std::string& config_file);
    void setup_signals();
    void handle_signal(int sig);
};

/// Template helper to run a service
template<typename CompositionT>
int run_service(int argc, char** argv)
{
    ServiceHost host;
    auto composition = std::make_unique<CompositionT>();
    return host.run(std::move(composition), argc, argv);
}

} // namespace pvpgn::runtime
