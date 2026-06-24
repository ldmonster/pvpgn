// SPDX-License-Identifier: GPL-2.0-or-later

/// @file health_handler.cpp
/// Implementation of HealthHandler.

#include "infra/health/health_handler.hpp"

#include <sstream>
#include <string>
#include <string_view>

namespace pvpgn::infra::health {

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void HealthHandler::enable_varz(std::function<std::string()> provider) {
    varz_provider_ = std::move(provider);
    varz_enabled_.store(true);
}

void HealthHandler::disable_varz() noexcept {
    varz_enabled_.store(false);
}

std::string HealthHandler::handle(std::string_view path) const {
    // GET /healthz — liveness probe: always 200 while the process is alive.
    if (path == "/healthz") {
        return make_response(200, "OK", R"({"status":"ok"})");
    }

    // GET /readyz — readiness probe: 200 once ready, 503 during startup.
    if (path == "/readyz") {
        if (ready_.load()) {
            return make_response(200, "OK", R"({"status":"ready"})");
        }
        return make_response(503, "Service Unavailable",
                             R"({"status":"starting"})");
    }

    // GET /varz — opt-in debug dump.
    if (path == "/varz") {
        if (!varz_enabled_.load()) {
            return make_response(404, "Not Found",
                                 R"({"error":"varz not enabled"})");
        }
        try {
            std::string body = varz_provider_ ? varz_provider_() : "{}";
            return make_response(200, "OK", body);
        } catch (...) {
            return make_response(500, "Internal Server Error",
                                 R"({"error":"varz provider threw"})");
        }
    }

    return make_response(404, "Not Found", R"({"error":"not found"})");
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

std::string HealthHandler::make_response(int status_code,
                                          std::string_view status_text,
                                          std::string_view body) {
    std::ostringstream oss;
    oss << "HTTP/1.1 " << status_code << " " << status_text << "\r\n"
        << "Content-Type: application/json\r\n"
        << "Content-Length: " << body.size() << "\r\n"
        << "Connection: close\r\n"
        << "\r\n"
        << body;
    return oss.str();
}

}  // namespace pvpgn::infra::health
