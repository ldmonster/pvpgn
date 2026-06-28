// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file d2dbs_tcp_session.hpp
/// Per-connection composition root for the v3 D2DBS server.
///
///   D2DBSTcpSession                    ← this class
///     ├── InMemoryCharacterSaveRepository
///     ├── InMemoryD2DBSLadderRepository
///     ├── D2DBSSessionHandler           (application layer)
///     ├── D2DBSSessionFsm               (protocol layer)
///     └── ID2DBSSessionEgress           (outbound port — implemented here)
///
/// The D2GS->D2DBS connection opens with a single 0x65 connect-class byte
/// (kConnectClassD2gsToD2dbs) before any framed packet; the session consumes it
/// once, then feeds the [size:2][type:2][seqno:4][body] frames to the FSM.
///
/// The egress interface is stateless (carries only a success flag), but the
/// SAVE_DATA_REPLY / GET_DATA_REPLY wire packets must echo the request's
/// seqno / datatype / char_name. The session therefore captures that context
/// from each FSM callback (wrapping the handler's callbacks) and uses it when
/// building the reply.

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "app/d2dbs/d2dbs_session_egress.hpp"
#include "app/d2dbs/d2dbs_session_handler.hpp"
#include "domain/d2dbs/in_memory_repositories.hpp"
#include "domain/d2dbs/types.hpp"
#include "protocol/d2dbs/fsm.hpp"

namespace pvpgn::infra::net { class TcpSession; }

namespace pvpgn::app::d2dbs {

class D2DBSTcpSession final
    : public ID2DBSSessionEgress
    , public std::enable_shared_from_this<D2DBSTcpSession> {
public:
    [[nodiscard]] static std::shared_ptr<D2DBSTcpSession>
    create(std::shared_ptr<infra::net::TcpSession> tcp) {
        return std::shared_ptr<D2DBSTcpSession>(
            new D2DBSTcpSession(std::move(tcp)));
    }

    D2DBSTcpSession(const D2DBSTcpSession&)            = delete;
    D2DBSTcpSession& operator=(const D2DBSTcpSession&) = delete;
    D2DBSTcpSession(D2DBSTcpSession&&)                 = delete;
    D2DBSTcpSession& operator=(D2DBSTcpSession&&)      = delete;

    ~D2DBSTcpSession() override = default;

    /// Wire callbacks and begin the async read loop.
    void start();

    // ----- ID2DBSSessionEgress -------------------------------------------
    void send_char_login_result(bool success) override;
    void send_char_logout_result(bool success) override;
    void send_char_save_result(bool success) override;
    void send_char_load_result(
        bool success,
        const domain::d2dbs::CharacterSaveData* data) override;
    void send_ladder_update_result(bool success) override;

private:
    explicit D2DBSTcpSession(std::shared_ptr<infra::net::TcpSession> tcp);

    void send_raw(std::vector<uint8_t> bytes);

    std::shared_ptr<infra::net::TcpSession>           tcp_;

    domain::d2dbs::InMemoryCharacterSaveRepository    save_repo_;
    domain::d2dbs::InMemoryD2DBSLadderRepository      ladder_repo_;

    std::unique_ptr<D2DBSSessionHandler>              handler_;
    std::unique_ptr<protocol::d2dbs::D2DBSSessionFsm> fsm_;

    bool          init_consumed_{false};

    /// Request context captured from the current FSM callback so the egress can
    /// build a wire-accurate SAVE_DATA_REPLY / GET_DATA_REPLY (which echo these).
    std::uint32_t cur_seqno_{0};
    std::uint16_t cur_datatype_{0};
    std::string   cur_char_name_;
};

}  // namespace pvpgn::app::d2dbs
