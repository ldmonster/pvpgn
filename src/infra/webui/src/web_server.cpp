// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/webui/web_server.hpp"

#include <sstream>

#include "domain/identity/ports.hpp"
#include "domain/chat/ports.hpp"
#include "domain/gameplay/ports.hpp"
#include "domain/identity/ports.hpp"
#include "application/ports/metrics_registry.hpp"
#include "infra/net/io_runtime.hpp"
#include "infra/webui/channel_json.hpp"
#include "infra/webui/dashboard_html.hpp"

namespace pvpgn::infra::webui {

class EmbeddedWebServer::Impl {
public:
    // Placeholder for actual HTTP server implementation
};

EmbeddedWebServer::EmbeddedWebServer(
    std::string_view bind_address,
    std::uint16_t port,
    std::shared_ptr<pvpgn::application::ports::ISessionRegistry> registry,
    std::shared_ptr<pvpgn::application::ports::IChannelRepository> channels,
    std::shared_ptr<pvpgn::application::ports::IGameRepository> games,
    std::shared_ptr<pvpgn::application::ports::IAccountRepository> accounts,
    std::shared_ptr<pvpgn::application::ports::IMetricsRegistry> metrics,
    pvpgn::infra::net::IoRuntime& runtime)
    : bind_address_(bind_address),
      port_(port),
      registry_(registry),
      channels_(channels),
      games_(games),
      accounts_(accounts),
      metrics_(metrics),
      runtime_(runtime),
      impl_(std::make_unique<Impl>()) {}

EmbeddedWebServer::~EmbeddedWebServer() = default;

void EmbeddedWebServer::start() {
    // Stub: Set up HTTP listener on bind_address_:port_
    // Register route handlers for /api/v1/* and /dashboard.html
}

void EmbeddedWebServer::stop() {
    // Stub: Close all connections and stop listener
}

std::string EmbeddedWebServer::handle_request(std::string_view method,
                                               std::string_view path,
                                               std::string_view body) {
    (void)method;
    (void)body;

    // Route to API or dashboard
    if (path == "/" || path == "") {
        return "HTTP/1.1 302 Found\r\nLocation: /dashboard.html\r\nContent-Length: 0\r\n\r\n";
    }

    if (path == "/dashboard.html") {
        std::string response = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n";
        response += "Content-Length: " + std::to_string(DASHBOARD_HTML.size()) + "\r\n\r\n";
        response += DASHBOARD_HTML;
        return response;
    }

    if (path.find("/api/v1/") == 0) {
        std::string json = route_api(path.substr(8));  // Skip "/api/v1/"
        std::string response = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n";
        response += "Content-Length: " + std::to_string(json.size()) + "\r\n\r\n";
        response += json;
        return response;
    }

    return "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
}

std::string EmbeddedWebServer::route_api(std::string_view path) {
    if (path == "status") {
        return get_status_json();
    }
    if (path == "players") {
        return get_players_json();
    }
    if (path == "channels") {
        return get_channels_json();
    }
    if (path == "games") {
        return get_games_json();
    }
    // Default: empty JSON object
    return "{}";
}

std::string EmbeddedWebServer::get_status_json() const {
    std::ostringstream oss;
    oss << "{"
        << "\"version\": \"pvpgn-v3.0.0\", "
        << "\"uptime_seconds\": 0, "
        << "\"active_connections\": " << (registry_ ? "0" : "0")
        << "}";
    return oss.str();
}

std::string EmbeddedWebServer::get_players_json() const {
    // Stub: Iterate registry and build player list
    return "[]";
}

std::string EmbeddedWebServer::get_channels_json() const {
    return channels_to_json(channels_.get());
}

std::string EmbeddedWebServer::get_games_json() const {
    // Stub: Iterate games and build game list
    return "[]";
}

}  // namespace pvpgn::infra::webui
