// SPDX-License-Identifier: GPL-2.0-or-later

/// @file metrics_handler.cpp
/// Implementation of MetricsHandler.

#include "infra/health/metrics_handler.hpp"
#include "core/metrics.hpp"

#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace pvpgn::infra::health {

MetricsHandler::MetricsHandler(
    std::shared_ptr<core::IMetricsRegistry> registry)
    : registry_(std::move(registry)) {
    if (!registry_) {
        throw std::invalid_argument("MetricsHandler: registry must not be null");
    }
}

std::string MetricsHandler::handle() const {
    std::string body;
    try {
        body = registry_->serialize();
    } catch (...) {
        body = "# error serializing metrics\n";
    }
    return make_response(body);
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

std::string MetricsHandler::make_response(std::string_view body) {
    std::ostringstream oss;
    oss << "HTTP/1.1 200 OK\r\n"
        << "Content-Type: text/plain; version=0.0.4; charset=utf-8\r\n"
        << "Content-Length: " << body.size() << "\r\n"
        << "Connection: close\r\n"
        << "\r\n"
        << body;
    return oss.str();
}

}  // namespace pvpgn::infra::health
