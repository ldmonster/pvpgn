// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// Service Configuration Loader
///
/// Loads and validates service configuration from TOML files.
/// Provides structured configuration for network, logging, and runtime settings.

#include "core/result.hpp"
#include "core/error.hpp"
#include <string>
#include <string_view>
#include <filesystem>
#include <unordered_map>
#include <optional>

using pvpgn::core::Result;
using pvpgn::core::Error;

namespace pvpgn::runtime {

/// Network configuration
struct NetworkConfig {
    /// Bind address (default: "0.0.0.0")
    std::string bind_address = "0.0.0.0";
    
    /// Port to listen on
    uint16_t port = 0;
    
    /// Enable TLS
    bool tls_enabled = false;
    
    /// Path to TLS certificate file
    std::string cert_file;
    
    /// Path to TLS private key file
    std::string key_file;
    
    /// Path to CA certificate file for client verification
    std::string ca_file;
};

/// Service runtime configuration
struct ServiceRuntimeConfig {
    /// Service name
    std::string service_name;
    
    /// Log level (trace, debug, info, warn, error, critical)
    std::string log_level = "info";
    
    /// Log file path (empty = stdout)
    std::string log_file;
    
    /// Run as daemon
    bool daemon = false;
    
    /// PID file path
    std::string pid_file;
    
    /// Run as user (Unix only)
    std::string run_as_user;
    
    /// Network configuration
    NetworkConfig network;
    
    /// Extra configuration parameters
    std::unordered_map<std::string, std::string> extra;
};

/// Service configuration loader
class ServiceConfigLoader {
public:
    /// Load configuration from TOML file
    /// @param path Path to TOML configuration file
    /// @return Loaded configuration or error
    static Result<ServiceRuntimeConfig, Error> load_toml(const std::filesystem::path& path);
    
    /// Load configuration from TOML string
    /// @param content TOML content as string
    /// @return Loaded configuration or error
    static Result<ServiceRuntimeConfig, Error> load_toml_string(std::string_view content);
    
    /// Validate configuration
    /// @param cfg Configuration to validate
    /// @return Success or validation error
    static Result<void, Error> validate(const ServiceRuntimeConfig& cfg);
};

} // namespace pvpgn::runtime
