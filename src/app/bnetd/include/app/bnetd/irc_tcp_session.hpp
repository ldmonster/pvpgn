// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file irc_tcp_session.hpp
/// `IrcTcpSession` — wires a `infra::net::TcpSession` to an `IrcFsm`.
///
/// Architecture
/// ------------
/// The IRC protocol is a line-oriented text protocol (RFC 1459):
///   * Client sends CRLF-terminated command lines.
///   * Server replies with CRLF-terminated numeric/command lines.
///   * Session ends when the client sends QUIT or the server closes.
///
/// `IrcTcpSession` owns the full object graph for one IRC connection:
///
///   IrcTcpSession (ISessionContext adapter)
///       ├─► infra::net::TcpSession  (Asio transport)
///       └─► IrcFsm                  (protocol state machine)
///
/// Unlike `BnftpTcpSession` which uses a separate egress-context chain,
/// `IrcTcpSession` directly implements `ISessionContext` because `IrcFsm`
/// takes `ISessionContext&` by reference (non-owning). The session object
/// itself is the context — it must outlive the FSM.
///
/// Thread-safety
/// -------------
/// All callbacks are invoked from Asio worker threads. The underlying
/// `TcpSession` serialises writes via its internal strand; `IrcFsm`
/// is single-threaded and must only be called from the Asio thread that
/// owns the session (guaranteed by `TcpSession`'s sequential callbacks).
///
/// Byte framing
/// ------------
/// IRC is line-oriented. `IrcTcpSession` accumulates received bytes in a
/// `std::string` buffer and calls `irc::try_parse_line` to extract
/// complete CRLF-terminated lines, then decodes each line with
/// `irc::decode` and feeds the resulting `Message` to `IrcFsm::handle`.

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "infra/net/tcp_session.hpp"
#include "protocol/irc/codec.hpp"
#include "protocol/irc/fsm.hpp"
#include "protocol/irc/message.hpp"
#include "protocol/irc/session_context.hpp"

namespace pvpgn::app::bnetd {

/// Maximum number of bytes that may accumulate in the line buffer before a
/// complete (CRLF-terminated) line is seen. IRC lines are bounded to 512 bytes
/// by RFC 1459; we allow a generous factor (mirroring `wol_fsm.cpp`'s
/// `kMaxLineLen * 4`) before treating the peer as hostile. An unauthenticated
/// client that streams bytes without ever sending a newline would otherwise
/// grow `rx_buf_` without bound (remote OOM / DoS).
inline constexpr std::size_t kIrcMaxLineLen = 512 * 4;  // 2048 bytes

/// Owns one IRC connection: TcpSession + IrcFsm.
///
/// Implements `protocol::irc::ISessionContext` so it can be passed
/// directly to `IrcFsm` as the context reference.
///
/// Lifetime: `shared_ptr` — the Asio callbacks keep the session alive
/// until the socket closes and all pending writes complete.
class IrcTcpSession
    : public std::enable_shared_from_this<IrcTcpSession>
    , public protocol::irc::ISessionContext {
public:
    /// Create a session from an accepted socket and a server name.
    ///
    /// @param tcp          Accepted TCP session (from `TcpSession::create`).
    /// @param server_name  Hostname announced in IRC numeric reply prefixes
    ///                     (e.g. ":pvpgn.server 001 nick :Welcome").
    IrcTcpSession(std::shared_ptr<infra::net::TcpSession> tcp,
                  std::string                              server_name)
        : tcp_(std::move(tcp))
        , server_name_(std::move(server_name))
        , fsm_(std::make_unique<protocol::irc::IrcFsm>(*this))
    {}

    IrcTcpSession(const IrcTcpSession&)            = delete;
    IrcTcpSession& operator=(const IrcTcpSession&) = delete;
    IrcTcpSession(IrcTcpSession&&)                 = delete;
    IrcTcpSession& operator=(IrcTcpSession&&)      = delete;

    ~IrcTcpSession() override = default;

    // -----------------------------------------------------------------------
    // ISessionContext implementation
    // -----------------------------------------------------------------------

    /// Encode `msg` to a CRLF-terminated wire line and send it.
    ///
    /// `irc::encode_to_string` already appends CRLF, so we convert the
    /// resulting string directly to a byte vector and hand it to TcpSession.
    core::Status<> send(const protocol::irc::Message& msg) override {
        if (!tcp_) {
            return core::fail(core::Error{core::StatusCode::Internal,
                                          "null tcp session"});
        }
        const std::string wire = protocol::irc::encode_to_string(msg);
        std::vector<std::byte> buf;
        buf.reserve(wire.size());
        for (char c : wire) {
            buf.push_back(static_cast<std::byte>(c));
        }
        tcp_->send(std::move(buf));
        return core::ok();
    }

    /// The server hostname used as the prefix on numeric replies.
    std::string_view server_name() const noexcept override {
        return server_name_;
    }

    /// Request orderly session shutdown — close the underlying socket.
    void close() override {
        if (tcp_) tcp_->close();
    }

    // -----------------------------------------------------------------------
    // Session lifecycle
    // -----------------------------------------------------------------------

    /// Wire callbacks and begin reading from the socket.
    ///
    /// Must be called exactly once after construction. After `start()` the
    /// session is self-sustaining: Asio callbacks hold a `shared_ptr` to
    /// `this` until the socket closes.
    void start() {
        auto self = shared_from_this();

        tcp_->set_on_bytes([self](core::ByteView bv) {
            self->on_bytes(bv);
        });

        tcp_->set_on_close([self](const boost::system::error_code&) {
            // Socket closed by peer — transition FSM to Closing if needed
            // IrcFsm has no explicit on_close(), so we just let it go.
            (void)self;
        });

        tcp_->start();
    }

    // -----------------------------------------------------------------------
    // Accessors (for testing / logging)
    // -----------------------------------------------------------------------

    /// Expose the FSM for state inspection in tests.
    [[nodiscard]] const protocol::irc::IrcFsm& fsm() const noexcept {
        return *fsm_;
    }

    /// Expose the configured server name.
    [[nodiscard]] const std::string& server_name_str() const noexcept {
        return server_name_;
    }

    /// Expose the current line-accumulation buffer size (for testing).
    [[nodiscard]] std::size_t rx_buf_size() const noexcept {
        return rx_buf_.size();
    }

    /// Accumulate inbound bytes and dispatch every complete CRLF-terminated
    /// line to the FSM.
    ///
    /// Guards against runaway clients: if the buffer grows past
    /// `kIrcMaxLineLen` without a newline being seen, the peer is treated as
    /// hostile — the buffer is cleared and the connection is closed, rather
    /// than allowing `rx_buf_` to grow without bound (remote OOM / DoS).
    ///
    /// Returns `false` if the session was closed due to overflow, `true`
    /// otherwise. Exposed (non-virtual) so tests can drive the framing path
    /// without a live Asio transport.
    bool on_bytes(core::ByteView bv) {
        // Accumulate bytes into the line buffer.
        rx_buf_.append(reinterpret_cast<const char*>(bv.data()), bv.size());

        // Extract and process complete lines.
        while (true) {
            auto frame_result = protocol::irc::try_parse_line(rx_buf_);
            if (!frame_result) break;  // NeedMore

            const auto& frame = frame_result.value();
            const std::string line{frame.line};
            rx_buf_.erase(0, frame.consumed);

            auto msg_result = protocol::irc::decode(line);
            if (!msg_result) continue;  // malformed line — skip

            (void)fsm_->handle(msg_result.value());
        }

        // After draining every complete line, whatever remains in `rx_buf_`
        // is an incomplete (unterminated) line. If it has grown past the cap
        // the peer is streaming bytes without ever sending a newline — treat
        // it as hostile and close rather than letting `rx_buf_` grow without
        // bound (remote OOM / DoS).
        if (rx_buf_.size() > kIrcMaxLineLen) {
            rx_buf_.clear();
            close();
            return false;
        }
        return true;
    }

private:
    std::shared_ptr<infra::net::TcpSession>    tcp_;
    std::string                                server_name_;
    std::unique_ptr<protocol::irc::IrcFsm>     fsm_;
    std::string                                rx_buf_;  ///< line accumulation buffer
};

}  // namespace pvpgn::app::bnetd
