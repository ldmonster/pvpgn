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
#include <spdlog/spdlog.h>

#include "application/ports/metrics_registry.hpp"
#include "core/logging.hpp"
#include "infra/net/io_runtime.hpp"

namespace pvpgn::infra::metrics {

namespace {
constexpr std::size_t BUFFER_SIZE = 4096;
constexpr std::size_t MAX_REQUEST_SIZE = 8192;

/// Minimal HTTP session handler
class HttpSession : public std::enable_shared_from_this<HttpSession> {
public:
    using tcp = boost::asio::ip::tcp;

    explicit HttpSession(tcp::socket socket,
                         std::shared_ptr<application::ports::IMetricsRegistry> registry)
        : socket_(std::move(socket)), registry_(registry) {}

    void start() { read_request(); }

private:
    tcp::socket socket_;
    std::shared_ptr<application::ports::IMetricsRegistry> registry_;
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
        } else {
            send_error(404);
        }
        return true;
    }

    void send_metrics() {
        std::string metrics = registry_->serialize();

        std::ostringstream response;
        response << "HTTP/1.1 200 OK\r\n";
        response << "Content-Type: text/plain; charset=utf-8\r\n";
        response << "Content-Length: " << metrics.size() << "\r\n";
        response << "Connection: close\r\n";
        response << "\r\n";
        response << metrics;

        auto response_str = response.str();
        auto self = shared_from_this();
        boost::asio::async_write(
            socket_,
            boost::asio::buffer(response_str),
            [self](const boost::system::error_code&, std::size_t) {
                // Close connection after response
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

        auto response_str = response.str();
        auto self = shared_from_this();
        boost::asio::async_write(
            socket_,
            boost::asio::buffer(response_str),
            [self](const boost::system::error_code&, std::size_t) {
                // Close connection after response
                self->socket_.close();
            });
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
         std::shared_ptr<application::ports::IMetricsRegistry> registry,
         boost::asio::io_context& io_ctx)
        : bind_address_(bind_address),
          port_(port),
          registry_(registry),
          acceptor_(io_ctx),
          io_ctx_(io_ctx) {}

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

            SPDLOG_INFO("HTTP metrics server listening on {}:{}", bind_address_, port_);
            accept_connection();
        } catch (const std::exception& e) {
            SPDLOG_ERROR("Failed to start HTTP metrics server: {}", e.what());
        }
    }

    void stop() {
        if (acceptor_.is_open()) {
            acceptor_.close();
            SPDLOG_INFO("HTTP metrics server stopped");
        }
    }

    bool is_running() const { return acceptor_.is_open(); }

private:
    std::string bind_address_;
    std::uint16_t port_;
    std::shared_ptr<application::ports::IMetricsRegistry> registry_;
    tcp::acceptor acceptor_;
    boost::asio::io_context& io_ctx_;

    void accept_connection() {
        if (!acceptor_.is_open()) {
            return;
        }

        acceptor_.async_accept(
            [this](const boost::system::error_code& ec, tcp::socket socket) {
                if (!ec) {
                    auto session =
                        std::make_shared<HttpSession>(std::move(socket), registry_);
                    session->start();
                } else {
                    SPDLOG_DEBUG("HTTP metrics server accept error: {}", ec.message());
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
                                     std::shared_ptr<application::ports::IMetricsRegistry> registry,
                                     infra::net::IoRuntime& runtime)
    : bind_address_(bind_address),
      port_(port),
      registry_(registry),
      runtime_(runtime),
      impl_(std::make_unique<Impl>(bind_address, port, registry, runtime.context())) {}

HttpMetricsServer::~HttpMetricsServer() {
    stop();
}

HttpMetricsServer::HttpMetricsServer(HttpMetricsServer&& other) noexcept
    : bind_address_(std::move(other.bind_address_)),
      port_(other.port_),
      registry_(other.registry_),
      runtime_(other.runtime_),
      impl_(std::move(other.impl_)) {}

HttpMetricsServer& HttpMetricsServer::operator=(HttpMetricsServer&& other) noexcept {
    if (this != &other) {
        stop();
        bind_address_ = std::move(other.bind_address_);
        port_ = other.port_;
        registry_ = other.registry_;
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

}  // namespace pvpgn::infra::metrics
