// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file udp_endpoint.hpp
/// A datagram socket bound to an address/port that pumps inbound
/// packets through a caller-supplied handler. Mirrors `TcpSession`
/// shape but for connectionless protocols — used by the BNet
/// "tracking" UDP probes (`handle_udp_packet`) and by future
/// admin/telnet datagram channels.
///
/// Handler contract
/// ----------------
///   * `on_datagram(remote, ByteView)` is called from a worker
///     thread every time a packet is received. The view is valid
///     only during the call.
///   * `on_error(error_code)` is called when an I/O error stops
///     reception. Reception is **not** automatically resumed; the
///     handler can `start()` again if appropriate.
///
/// Writes are queued in a `std::deque<Datagram>` and serialised via
/// a single in-flight `async_send_to`. The endpoint never blocks the
/// caller; from any thread, `send_to()` is fire-and-forget.

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include <boost/asio/ip/udp.hpp>
#include <boost/asio/strand.hpp>

#include "core/bytes.hpp"
#include "core/error.hpp"
#include "core/result.hpp"
#include "infra/net/io_runtime.hpp"

namespace pvpgn::infra::net {

class UdpEndpoint {
public:
    using OnDatagram = std::function<
        void(const boost::asio::ip::udp::endpoint&, core::ByteView)>;
    using OnError = std::function<void(const boost::system::error_code&)>;

    explicit UdpEndpoint(IoRuntime& rt);
    ~UdpEndpoint();

    UdpEndpoint(const UdpEndpoint&)            = delete;
    UdpEndpoint& operator=(const UdpEndpoint&) = delete;

    void set_on_datagram(OnDatagram cb) { on_datagram_ = std::move(cb); }
    void set_on_error(OnError cb)       { on_error_    = std::move(cb); }

    /// Bind to the given UDP endpoint. Returns the bound endpoint
    /// (useful when port=0 lets the OS pick).
    core::Result<boost::asio::ip::udp::endpoint>
    bind(const boost::asio::ip::udp::endpoint& ep);

    /// Convenience: parse a `host:port` pair. `host` may be
    /// `0.0.0.0`, `::`, or a specific interface address.
    core::Result<boost::asio::ip::udp::endpoint>
    bind(const std::string& host, std::uint16_t port);

    /// Adopt an already-open native UDP socket descriptor. The
    /// caller must have completed any `socket(2)`/`bind(2)`/
    /// `setsockopt(2)`/non-blocking setup it cares about. After
    /// this call the endpoint owns the descriptor — do **not**
    /// `close(2)` it from outside. Used to take over a UDP fd that
    /// the legacy bnetd server already opened and bound.
    core::Result<boost::asio::ip::udp::endpoint>
    adopt_native_handle(int fd, bool ipv6 = false);

    /// Begin reading. Idempotent: a second call after `close()` is
    /// a no-op until a fresh `bind()`.
    void start();

    /// Send @p bytes to @p remote. Safe from any thread.
    void send_to(boost::asio::ip::udp::endpoint remote,
                 std::vector<std::byte> bytes);

    /// Close the socket. Idempotent.
    void close();

    boost::asio::ip::udp::endpoint local_endpoint() const;

private:
    struct OutItem {
        boost::asio::ip::udp::endpoint to;
        std::vector<std::byte>         bytes;
    };

    void do_receive();
    void do_send_locked();

    IoRuntime&                                                  rt_;
    boost::asio::ip::udp::socket                                socket_;
    boost::asio::strand<boost::asio::any_io_executor>           strand_;
    std::array<std::byte, 2048>                                 rx_buf_{};
    boost::asio::ip::udp::endpoint                              rx_from_{};
    std::deque<OutItem>                                         tx_q_;
    bool                                                        sending_ = false;
    bool                                                        receiving_ = false;
    bool                                                        closed_ = false;
    std::mutex                                                  mu_;
    OnDatagram                                                  on_datagram_;
    OnError                                                     on_error_;
};

}  // namespace pvpgn::infra::net
