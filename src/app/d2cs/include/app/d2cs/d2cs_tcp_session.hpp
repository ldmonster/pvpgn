// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file d2cs_tcp_session.hpp
/// `D2CSTcpSession` — per-connection object that owns the D2CS protocol
/// stack and wires it to an Asio TCP socket.
///
/// ## Architecture
///
///   TcpSession (infra/net)
///     │  on_bytes / on_close callbacks
///     ▼
///   D2CSTcpSession                    ← this class
///     │  owns
///     ├── InMemoryCharacterRepository  (placeholder — real FS repo in later round)
///     ├── InMemoryLadderRepository     (placeholder — real FS repo in later round)
///     ├── D2CSSessionHandler           (application layer)
///     └── D2CSSessionFsm              (protocol layer)
///     │  implements
///     └── ID2CSSessionEgress          (outbound port)
///
/// ## Egress serialisation
///
/// The production wire format for each reply is built using the static
/// packet-builder methods on `D2CSSessionFsm` (e.g. `make_login_reply`,
/// `make_char_list_reply`). For replies that have no builder yet (ladder,
/// char-select) a minimal 3-byte stub header is sent so the client does
/// not hang waiting for a response.
///
/// ## Lifetime
///
/// `D2CSTcpSession` is heap-allocated and kept alive by the
/// `shared_ptr<infra::net::TcpSession>` callbacks. When the TCP session
/// closes, the callbacks are released and the object is destroyed.
///
/// ## C++20 conventions
///   - No exceptions; errors logged and dropped
///   - `[[nodiscard]]` on factory functions
///   - `-std=c++20 -Wall -Wextra -Werror` clean

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "app/d2cs/d2cs_session_egress.hpp"
#include "app/d2cs/d2cs_session_handler.hpp"
#include "app/d2cs/d2gs_registry.hpp"
#include "domain/d2cs/in_memory_repositories.hpp"
#include "domain/d2cs/types.hpp"
#include "protocol/d2cs/fsm.hpp"

// Forward-declare TcpSession to avoid pulling infra_net (Boost) into app_d2cs.
// The full definition is only needed in d2cs_tcp_session.cpp, which is
// compiled as part of pvpgn_v3_d2cs (not app_d2cs).
namespace pvpgn::infra::net { class TcpSession; }

namespace pvpgn::app::d2cs {

// ---------------------------------------------------------------------------
// D2CSTcpSession
// ---------------------------------------------------------------------------

/// Per-connection object that owns the full D2CS protocol stack.
///
/// Construct via `D2CSTcpSession::create()` and call `start()` to begin
/// the async read loop.
class D2CSTcpSession final
    : public ID2CSSessionEgress
    , public std::enable_shared_from_this<D2CSTcpSession> {
public:
    /// Factory: create a session for an accepted TCP connection.
    ///
    /// @param tcp       Shared TcpSession wrapping the accepted socket.
    /// @param registry  Shared D2GS link/game registry (may be null in tests).
    /// @return          Shared pointer to the new session (not yet started).
    [[nodiscard]] static std::shared_ptr<D2CSTcpSession>
    create(std::shared_ptr<infra::net::TcpSession> tcp,
           std::shared_ptr<D2gsRegistry> registry = nullptr) {
        // Cannot use make_shared because constructor is private.
        return std::shared_ptr<D2CSTcpSession>(
            new D2CSTcpSession(std::move(tcp), std::move(registry)));
    }

    D2CSTcpSession(const D2CSTcpSession&)            = delete;
    D2CSTcpSession& operator=(const D2CSTcpSession&) = delete;
    D2CSTcpSession(D2CSTcpSession&&)                 = delete;
    D2CSTcpSession& operator=(D2CSTcpSession&&)      = delete;

    ~D2CSTcpSession() = default;

    /// Wire callbacks and begin the async read loop.
    void start();

    // -----------------------------------------------------------------------
    // Cross-session game-lobby routing (called between sessions via the
    // shared D2gsRegistry). Public so a client session can drive a D2GS link
    // session and vice-versa.
    // -----------------------------------------------------------------------

    /// Forward a framed request (e.g. D2CS_D2GS_CREATEGAMEREQ 0x20) to this
    /// D2GS link, tagging the frame seqno with the correlation id.
    void send_d2gs_request(std::uint16_t type, std::uint32_t corr,
                          const std::vector<uint8_t>& body);

    /// Send a CREATEGAMEREPLY (0x03) to this client session.
    void send_create_game_reply(std::uint16_t client_seqno,
                                std::uint32_t game_id, std::uint32_t result,
                                std::uint16_t u1);

    /// Send a JOINGAMEREPLY (0x04) to this client session.
    void send_join_game_reply(std::uint16_t client_seqno, std::uint32_t game_id,
                              std::uint32_t gs_ip, std::uint32_t token,
                              std::uint32_t result);

    // -----------------------------------------------------------------------
    // ID2CSSessionEgress implementation
    // -----------------------------------------------------------------------

    void send_char_list(
        const std::vector<domain::d2cs::CharacterInfo>& chars) override;

    void send_char_list_110(
        const std::vector<domain::d2cs::CharacterInfo>& chars) override;

    void send_char_list_result(bool success) override;

    void send_char_select_result(
        bool success,
        const domain::d2cs::CharacterInfo* info) override;

    void send_char_create_result(
        domain::d2cs::CharacterCreateResult result) override;

    void send_char_delete_result(bool success) override;

    void send_ladder(
        const std::vector<domain::d2cs::LadderEntry>& entries) override;

    void send_realm_logon_result(
        domain::d2cs::RealmLogonResult result) override;

    void send_motd(std::string_view message) override;

private:
    D2CSTcpSession(std::shared_ptr<infra::net::TcpSession> tcp,
                   std::shared_ptr<D2gsRegistry> registry);

    /// Route a client CREATEGAMEREQ: pick a D2GS, forward 0x20 + record a
    /// pending entry, or reply FAILED when no D2GS is available.
    core::Result<void, core::Error> route_create_game(
        const protocol::d2cs::D2CSCreateGameRequest& req);

    /// Route a client JOINGAMEREQ: look up the game by name, forward 0x21 to its
    /// host D2GS + record a pending entry, or reply FAILED when not found.
    core::Result<void, core::Error> route_join_game(
        const protocol::d2cs::D2CSJoinGameRequest& req);

    /// Send raw bytes over the TCP socket.
    void send_raw(std::vector<uint8_t> bytes);

    // -----------------------------------------------------------------------
    // D2GS server-to-server link (init class CLIENT_INITCONN_CLASS_D2GS = 0x64)
    //
    // A game server (D2GS) connects to d2cs on the same listener with a 0x64
    // init byte. d2cs immediately sends AUTHREQ (0x10); the D2GS answers with
    // AUTHREPLY (0x11, version/checksum/sign); d2cs replies AUTHREPLY (0x11,
    // result). With version/checksum checks disabled (the defaults) the result
    // is SUCCEED. The link uses an 8-byte [size:2][type:2][seqno:4] header,
    // distinct from the client's 3-byte framing, so it is handled inline here
    // rather than via the client FSM.
    // -----------------------------------------------------------------------

    /// Send AUTHREQ to the freshly-connected D2GS (begins the link handshake).
    void start_d2gs_link();
    /// Buffer + dispatch framed D2GS->D2CS packets.
    void feed_d2gs(const uint8_t* data, std::size_t size);
    /// Send a single framed D2CS->D2GS packet.
    void send_d2gs_frame(std::uint16_t type, std::uint32_t seqno,
                         const std::vector<uint8_t>& body);

    // -----------------------------------------------------------------------
    // Owned objects (per-session state)
    // -----------------------------------------------------------------------

    std::shared_ptr<infra::net::TcpSession>      tcp_;

    /// Placeholder repositories — replaced by filesystem repos in a later round.
    domain::d2cs::InMemoryCharacterRepository    char_repo_;
    domain::d2cs::InMemoryLadderRepository       ladder_repo_;

    /// Application layer: translates FSM callbacks to use-case calls.
    /// Constructed after char_repo_ / ladder_repo_ / egress_ are ready.
    std::unique_ptr<D2CSSessionHandler>          handler_;

    /// Protocol layer: reassembles D2CS packets and fires callbacks.
    std::unique_ptr<protocol::d2cs::D2CSSessionFsm> fsm_;

    /// A D2 client opens the connection by sending a single init class byte
    /// (CLIENT_INITCONN_CLASS_D2CS = 0x01) BEFORE any framed packet, exactly as
    /// it does for the BNCS/BNFTP listeners. The original d2cs consumes this in
    /// handle_init; the FSM here only understands framed packets, so the session
    /// strips the leading byte before feeding the stream. False until consumed.
    bool init_consumed_ = false;

    /// True once the init byte was 0x64 (CLIENT_INITCONN_CLASS_D2GS): this
    /// connection is a D2GS server link, not a D2 client.
    bool d2gs_link_ = false;
    /// Reassembly buffer for framed D2GS->D2CS packets.
    std::vector<uint8_t> d2gs_buf_;
    /// Session number assigned to this D2GS link (echoed in AUTHREQ).
    std::uint32_t d2gs_sessionnum_ = 0;
    /// True once this D2GS link has been registered as choosable (post
    /// SETGSINFO), so it is registered/removed exactly once.
    bool d2gs_registered_ = false;

    /// Shared cross-session game-lobby registry (null when unused).
    std::shared_ptr<D2gsRegistry> registry_;
};

} // namespace pvpgn::app::d2cs
