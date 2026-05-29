// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file web_server.hpp
/// Embedded REST API server with static HTML dashboard.
/// Serves JSON endpoints and a single-file HTML dashboard.

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

// Forward declarations
namespace pvpgn::application::ports {
class ISessionRegistry;
class IChannelRepository;
class IGameRepository;
class IAccountRepository;
class IMetricsRegistry;
}
namespace pvpgn::infra::net {
class IoRuntime;
}

namespace pvpgn::infra::webui {

/// Embedded web server with REST API and dashboard.
class EmbeddedWebServer {
public:
    /// Create and configure the web server.
    ///
    /// Routes:
    /// - GET /api/v1/status    → JSON server status
    /// - GET /api/v1/players   → JSON list of online players
    /// - GET /api/v1/channels  → JSON list of active channels
    /// - GET /api/v1/games     → JSON list of active games
    /// - GET /api/v1/metrics   → Prometheus text format
    /// - GET /                 → redirect to /dashboard.html
    /// - GET /dashboard.html   → embedded HTML dashboard
    EmbeddedWebServer(
        std::string_view bind_address,
        std::uint16_t port,
        std::shared_ptr<pvpgn::application::ports::ISessionRegistry> registry,
        std::shared_ptr<pvpgn::application::ports::IChannelRepository> channels,
        std::shared_ptr<pvpgn::application::ports::IGameRepository> games,
        std::shared_ptr<pvpgn::application::ports::IAccountRepository> accounts,
        std::shared_ptr<pvpgn::application::ports::IMetricsRegistry> metrics,
        pvpgn::infra::net::IoRuntime& runtime);

    ~EmbeddedWebServer();

    EmbeddedWebServer(const EmbeddedWebServer&) = delete;
    EmbeddedWebServer& operator=(const EmbeddedWebServer&) = delete;
    EmbeddedWebServer(EmbeddedWebServer&&) noexcept = delete;
    EmbeddedWebServer& operator=(EmbeddedWebServer&&) noexcept = delete;

    /// Start the HTTP server.
    void start();

    /// Stop the HTTP server.
    void stop();

private:
    std::string bind_address_;
    std::uint16_t port_;
    std::shared_ptr<pvpgn::application::ports::ISessionRegistry> registry_;
    std::shared_ptr<pvpgn::application::ports::IChannelRepository> channels_;
    std::shared_ptr<pvpgn::application::ports::IGameRepository> games_;
    std::shared_ptr<pvpgn::application::ports::IAccountRepository> accounts_;
    std::shared_ptr<pvpgn::application::ports::IMetricsRegistry> metrics_;
    pvpgn::infra::net::IoRuntime& runtime_;

    class Impl;
    std::unique_ptr<Impl> impl_;

    // Request routing
    std::string handle_request(std::string_view method, std::string_view path,
                               std::string_view body);

    // API endpoints
    std::string route_api(std::string_view path);
    std::string get_status_json() const;
    std::string get_players_json() const;
    std::string get_channels_json() const;
    std::string get_games_json() const;
};

}  // namespace pvpgn::infra::webui
