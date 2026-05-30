// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file health_handler.hpp
/// HTTP handler for liveness and readiness probes (Plan 10 §12–13).
///
/// Routes:
///   GET /healthz  — liveness:  always 200 {"status":"ok"} while the process
///                               is alive and the event loop is responsive.
///   GET /readyz   — readiness: 200 {"status":"ready"} once set_ready(true)
///                               has been called; 503 {"status":"starting"}
///                               before that.
///   GET /varz     — debug dump (opt-in, Plan 10 §14): TOML snapshot, plugin
///                               list, Lua VM stats.  Returns 404 unless
///                               enabled via enable_varz().
///
/// This handler is intentionally decoupled from any specific HTTP server
/// implementation.  The `handle()` method takes a path string and returns a
/// pre-formatted HTTP/1.1 response string (headers + body) so it can be
/// embedded in any TCP session loop.

#include <atomic>
#include <functional>
#include <string>
#include <string_view>

namespace pvpgn::infra::health {

/// Produces HTTP/1.1 response strings for health probe endpoints.
class HealthHandler {
public:
    HealthHandler() = default;

    /// Signal that the server has finished startup and is ready to serve
    /// traffic.  Until this is called, /readyz returns 503.
    void set_ready(bool ready) noexcept { ready_.store(ready); }

    /// Returns true if set_ready(true) has been called.
    [[nodiscard]] bool is_ready() const noexcept { return ready_.load(); }

    /// Opt-in to the /varz debug endpoint.
    /// @param provider  Callable that returns the TOML/JSON debug dump string.
    ///                  Called on every /varz request; must be thread-safe.
    void enable_varz(std::function<std::string()> provider);

    /// Disable the /varz endpoint (default state).
    void disable_varz() noexcept;

    /// Handle an HTTP GET request for a health probe path.
    ///
    /// @param path  The request path (e.g. "/healthz", "/readyz", "/varz").
    /// @returns     A complete HTTP/1.1 response string (status line + headers
    ///              + blank line + body), ready to write to a TCP socket.
    ///              Returns a 404 response for unknown paths.
    [[nodiscard]] std::string handle(std::string_view path) const;

private:
    std::atomic<bool>              ready_{false};
    std::function<std::string()>   varz_provider_;
    mutable std::atomic<bool>      varz_enabled_{false};

    static std::string make_response(int status_code,
                                     std::string_view status_text,
                                     std::string_view body);
};

}  // namespace pvpgn::infra::health
