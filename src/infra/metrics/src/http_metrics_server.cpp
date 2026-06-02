// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/metrics/http_metrics_server.hpp"

#include <algorithm>
#include <array>
#include <memory>
#include <sstream>
#include <vector>

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include "core/metrics.hpp"
#include "core/logging.hpp"
#include "core/version.hpp"
#include "infra/net/io_runtime.hpp"

namespace pvpgn::infra::metrics {

namespace {
constexpr std::size_t BUFFER_SIZE = 4096;
constexpr std::size_t MAX_REQUEST_SIZE = 8192;

// ---------------------------------------------------------------------------
// Version / build metadata
// ---------------------------------------------------------------------------

/// Git hash injected at build time via -DPVPGN_GIT_HASH=<hash>.
/// Falls back to "unknown" when not set.
#ifndef PVPGN_GIT_HASH
#  define PVPGN_GIT_HASH "unknown"
#endif

/// Build a JSON version object once.
inline std::string make_version_json() {
    std::ostringstream oss;
    oss << "{"
        << "\"version\":\"" << pvpgn::core::kVersionString << "\","
        << "\"build_date\":\"" << __DATE__ << "\","
        << "\"git_hash\":\"" << PVPGN_GIT_HASH << "\""
        << "}";
    return oss.str();
}

// ---------------------------------------------------------------------------
// Minimal HTTP session handler
// ---------------------------------------------------------------------------

class HttpSession : public std::enable_shared_from_this<HttpSession> {
public:
    using tcp = boost::asio::ip::tcp;

    explicit HttpSession(tcp::socket socket,
                         std::shared_ptr<core::IMetricsRegistry> registry,
                         const std::atomic<bool>& ready_flag)
        : socket_(std::move(socket))
        , registry_(registry)
        , ready_flag_(ready_flag) {}

    void start() { read_request(); }

private:
    tcp::socket socket_;
    std::shared_ptr<core::IMetricsRegistry> registry_;
    const std::atomic<bool>& ready_flag_;
    std::array<char, BUFFER_SIZE> buffer_{};
    std::string request_data_;

    void read_request() {
        auto self = shared_from_this();
        socket_.async_read_some(
            boost::asio::buffer(buffer_),
            [self](const boost::system::error_code& ec, std::size_t bytes_transferred) {
                if (!ec) {
                    self->request_data_.append(self->buffer_.data(), bytes_transferred);
                    if (self->request_data_.size() > MAX_REQUEST_SIZE) {
                        self->send_error(413);
                    } else if (self->parse_and_respond()) {
                        // Response sent
                    } else if (self->request_data_.find("\r\n\r\n") == std::string::npos) {
                        // Continue reading more data
                        self->read_request();
                    }
                }
                // On error or completion, session ends (destructor called)
            });
    }

    bool parse_and_respond() {
        // Look for end of request line
        auto eol = request_data_.find("\r\n");
        if (eol == std::string::npos) {
            return false;  // Need more data
        }

        // Parse request line: "METHOD PATH HTTP/VERSION"
        std::string request_line = request_data_.substr(0, eol);
        std::istringstream iss(request_line);
        std::string method, path, http_version;
        iss >> method >> path >> http_version;

        if (method != "GET") {
            send_error(405);  // Method not allowed
            return true;
        }

        if (path == "/metrics") {
            send_metrics();
        } else if (path == "/healthz") {
            send_json(200, "{\"status\":\"ok\"}");
        } else if (path == "/readyz") {
            if (ready_flag_.load(std::memory_order_acquire)) {
                send_json(200, "{\"status\":\"ready\"}");
            } else {
                send_json(503, "{\"status\":\"starting\"}");
            }
        } else if (path == "/version") {
            static const std::string version_json = make_version_json();
            send_json(200, version_json);
        } else if (path == "/config/effective") {
            // Stub: return minimal config JSON with secrets redacted.
            send_json(200, "{\"note\":\"effective config not yet wired\",\"secrets\":\"***\"}");
        } else {
            send_error(404);
        }
        return true;
    }

    void send_metrics() {
        std::string metrics = registry_->serialize();

        std::ostringstream response;
        response << "HTTP/1.1 200 OK\r\n";
        response << "Content-Type: text/plain; version=0.0.4; charset=utf-8\r\n";
        response << "Content-Length: " << metrics.size() << "\r\n";
        response << "Connection: close\r\n";
        response << "\r\n";
        response << metrics;

        send_raw(response.str());
    }

    void send_json(int status_code, const std::string& body) {
        std::ostringstream response;
        switch (status_code) {
            case 200: response << "HTTP/1.1 200 OK\r\n"; break;
            case 503: response << "HTTP/1.1 503 Service Unavailable\r\n"; break;
            default:  response << "HTTP/1.1 200 OK\r\n"; break;
        }
        response << "Content-Type: application/json\r\n";
        response << "Content-Length: " << body.size() << "\r\n";
        response << "Connection: close\r\n";
        response << "\r\n";
        response << body;
        send_raw(response.str());
    }

    void send_raw(std::string data) {
        auto self = shared_from_this();
        // Keep the string alive until the async_write completes.
        auto buf = std::make_shared<std::string>(std::move(data));
        boost::asio::async_write(
            socket_,
            boost::asio::buffer(*buf),
            [self, buf](const boost::system::error_code&, std::size_t) {
                self->socket_.close();
            });
    }

    void send_error(int status_code) {
        std::ostringstream response;
        std::string message;

        switch (status_code) {
            case 404:
                response << "HTTP/1.1 404 Not Found\r\n";
                message = "Not Found";
                break;
            case 405:
                response << "HTTP/1.1 405 Method Not Allowed\r\n";
                message = "Method Not Allowed";
                break;
            case 413:
                response << "HTTP/1.1 413 Payload Too Large\r\n";
                message = "Request Entity Too Large";
                break;
            default:
                response << "HTTP/1.1 500 Internal Server Error\r\n";
                message = "Internal Server Error";
        }

        response << "Content-Type: text/plain\r\n";
        response << "Content-Length: " << message.size() << "\r\n";
        response << "Connection: close\r\n";
        response << "\r\n";
        response << message;

        send_raw(response.str());
    }
};

}  // anonymous namespace

// ============================================================================
// HttpMetricsServer::Impl
// ============================================================================

class HttpMetricsServer::Impl {
public:
    using tcp = boost::asio::ip::tcp;

    Impl(std::string_view bind_address,
         std::uint16_t port,
         std::shared_ptr<core::IMetricsRegistry> registry,
         boost::asio::io_context& io_ctx,
         const std::atomic<bool>& ready_flag)
        : bind_address_(bind_address),
          port_(port),
          registry_(registry),
          acceptor_(io_ctx),
          io_ctx_(io_ctx),
          ready_flag_(ready_flag) {}

    void start() {
        if (acceptor_.is_open()) {
            return;  // Already running
        }

        try {
            tcp::endpoint endpoint(
                boost::asio::ip::address::from_string(std::string(bind_address_)), port_);
            acceptor_.open(endpoint.protocol());
            acceptor_.set_option(boost::asio::socket_base::reuse_address(true));
            acceptor_.bind(endpoint);
            acceptor_.listen(boost::asio::socket_base::max_listen_connections);

            core::log(core::LogLevel::Info, "infra_metrics",
                      "HTTP metrics server listening on " +
                      std::string(bind_address_) + ":" + std::to_string(port_));
            accept_connection();
        } catch (const std::exception& e) {
            core::log(core::LogLevel::Error, "infra_metrics",
                      std::string("Failed to start HTTP metrics server: ") + e.what());
        }
    }

    void stop() {
        if (acceptor_.is_open()) {
            acceptor_.close();
            core::log(core::LogLevel::Info, "infra_metrics",
                      "HTTP metrics server stopped");
        }
    }

    bool is_running() const { return acceptor_.is_open(); }

private:
    std::string bind_address_;
    std::uint16_t port_;
    std::shared_ptr<core::IMetricsRegistry> registry_;
    tcp::acceptor acceptor_;
    boost::asio::io_context& io_ctx_;
    const std::atomic<bool>& ready_flag_;

    void accept_connection() {
        if (!acceptor_.is_open()) {
            return;
        }

        acceptor_.async_accept(
            [this](const boost::system::error_code& ec, tcp::socket socket) {
                if (!ec) {
                    auto session = std::make_shared<HttpSession>(
                        std::move(socket), registry_, ready_flag_);
                    session->start();
                } else {
                    core::log(core::LogLevel::Debug, "infra_metrics",
                              "HTTP metrics server accept error: " + ec.message());
                }

                if (acceptor_.is_open()) {
                    accept_connection();
                }
            });
    }
};

// ============================================================================
// HttpMetricsServer
// ============================================================================

HttpMetricsServer::HttpMetricsServer(std::string_view bind_address,
                                     std::uint16_t port,
                                     std::shared_ptr<core::IMetricsRegistry> registry,
                                     infra::net::IoRuntime& runtime)
    : bind_address_(bind_address),
      port_(port),
      registry_(registry),
      runtime_(runtime),
      ready_(false),
      impl_(std::make_unique<Impl>(bind_address, port, registry, runtime.context(), ready_)) {}

HttpMetricsServer::~HttpMetricsServer() {
    stop();
}

HttpMetricsServer::HttpMetricsServer(HttpMetricsServer&& other) noexcept
    : bind_address_(std::move(other.bind_address_)),
      port_(other.port_),
      registry_(other.registry_),
      runtime_(other.runtime_),
      ready_(other.ready_.load(std::memory_order_relaxed)),
      impl_(std::move(other.impl_)) {}

HttpMetricsServer& HttpMetricsServer::operator=(HttpMetricsServer&& other) noexcept {
    if (this != &other) {
        stop();
        bind_address_ = std::move(other.bind_address_);
        port_ = other.port_;
        registry_ = other.registry_;
        ready_.store(other.ready_.load(std::memory_order_relaxed), std::memory_order_relaxed);
        impl_ = std::move(other.impl_);
    }
    return *this;
}

void HttpMetricsServer::start() {
    if (impl_) {
        impl_->start();
    }
}

void HttpMetricsServer::stop() {
    if (impl_) {
        impl_->stop();
    }
}

void HttpMetricsServer::set_ready(bool ready) noexcept {
    ready_.store(ready, std::memory_order_release);
}

bool HttpMetricsServer::is_ready() const noexcept {
    return ready_.load(std::memory_order_acquire);
}

}  // namespace pvpgn::infra::metrics
