#pragma once
#include <string>
#include <cstdint>
#include <optional>
#include <chrono>

namespace pvpgn::infra::discovery {

/// Transport protocol for a service endpoint
enum class Transport { TCP, UDP, Unix };

/// A single service endpoint (address + port + metadata)
struct ServiceEndpoint {
    std::string service_name;       ///< e.g. "bnetd", "d2cs", "d2dbs"
    std::string host;               ///< IP or hostname
    uint16_t port{0};
    Transport transport{Transport::TCP};
    std::string version;            ///< Service version string
    std::string instance_id;        ///< Unique instance ID (UUID or hostname:port)
    std::chrono::steady_clock::time_point registered_at;
    std::chrono::seconds ttl{30};   ///< Time-to-live for health checks
    bool healthy{true};

    /// Format as "host:port"
    [[nodiscard]] std::string address() const;
};

} // namespace pvpgn::infra::discovery
