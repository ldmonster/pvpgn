// SPDX-License-Identifier: GPL-2.0-or-later

/// @file d2cs_tcp_session.cpp
/// Implementation of `D2CSTcpSession`.
///
/// Egress serialisation uses the static packet-builder methods on
/// `D2CSSessionFsm` where they exist. For replies that have no dedicated
/// builder yet (char-select, ladder) a minimal 3-byte stub header is sent
/// so the client does not hang. The exact wire format will be refined in
/// later rounds.

#include "app/d2cs/d2cs_tcp_session.hpp"

// Full definition of TcpSession — only available in the binary (infra_net).
// The header uses a forward declaration to keep app_d2cs Boost-free.
#include "infra/net/tcp_session.hpp"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <boost/asio/error.hpp>
#include <boost/system/error_code.hpp>

#include "core/bytes.hpp"
#include "domain/d2cs/types.hpp"
#include "protocol/d2cs/fsm.hpp"

namespace pvpgn::app::d2cs {

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

D2CSTcpSession::D2CSTcpSession(std::shared_ptr<infra::net::TcpSession> tcp)
    : tcp_(std::move(tcp))
    , char_repo_{}
    , ladder_repo_{}
    , handler_(std::make_unique<D2CSSessionHandler>(
          char_repo_, ladder_repo_, *this))
    , fsm_(std::make_unique<protocol::d2cs::D2CSSessionFsm>(
          handler_->make_callbacks()))
{}

// ---------------------------------------------------------------------------
// start()
// ---------------------------------------------------------------------------

void D2CSTcpSession::start() {
    auto self = shared_from_this();

    tcp_->set_on_bytes([self](core::ByteView bv) {
        // Feed raw bytes into the FSM reassembly buffer.
        const auto* data = reinterpret_cast<const uint8_t*>(bv.data());
        auto result = self->fsm_->feed(data, bv.size());
        if (!result) {
            // Malformed packet or callback error — close the session.
            std::cerr << "[d2cs] session error: "
                      << result.error().message() << "\n";
            self->tcp_->close();
        }
    });

    tcp_->set_on_close([self](const boost::system::error_code& ec) {
        if (ec && ec != boost::asio::error::eof) {
            std::cerr << "[d2cs] session closed: " << ec.message() << "\n";
        }
        // FSM disconnect callback fires via on_disconnect in callbacks.
    });

    tcp_->start();
}

// ---------------------------------------------------------------------------
// send_raw() — internal helper
// ---------------------------------------------------------------------------

void D2CSTcpSession::send_raw(std::vector<uint8_t> bytes) {
    if (!tcp_) return;
    std::vector<std::byte> buf;
    buf.reserve(bytes.size());
    for (auto b : bytes) {
        buf.push_back(static_cast<std::byte>(b));
    }
    tcp_->send(std::move(buf));
}

// ---------------------------------------------------------------------------
// ID2CSSessionEgress implementation
// ---------------------------------------------------------------------------

void D2CSTcpSession::send_realm_logon_result(
    domain::d2cs::RealmLogonResult result) {
    const auto code = static_cast<uint32_t>(result);
    send_raw(protocol::d2cs::D2CSSessionFsm::make_login_reply(code));
}

void D2CSTcpSession::send_char_list(
    const std::vector<domain::d2cs::CharacterInfo>& chars) {
    // Build the character name list for the FSM packet builder.
    std::vector<std::string> names;
    names.reserve(chars.size());
    for (const auto& c : chars) {
        names.push_back(c.name);
    }
    send_raw(protocol::d2cs::D2CSSessionFsm::make_char_list_reply(names));
}

void D2CSTcpSession::send_char_list_result(bool success) {
    // Send an empty char list on failure (client will show "no characters").
    if (!success) {
        send_raw(protocol::d2cs::D2CSSessionFsm::make_char_list_reply({}));
    }
    // On success the caller should have called send_char_list() instead.
}

void D2CSTcpSession::send_char_select_result(
    bool success,
    const domain::d2cs::CharacterInfo* /*info*/) {
    // result_code: 0x00 = success, 0x46 = not found
    const uint32_t code = success ? 0x00u : 0x46u;
    send_raw(protocol::d2cs::D2CSSessionFsm::make_char_login_reply(code));
}

void D2CSTcpSession::send_char_create_result(bool success) {
    // result_code: 0x00 = success, 0x01 = failed
    const uint32_t code = success ? 0x00u : 0x01u;
    send_raw(protocol::d2cs::D2CSSessionFsm::make_create_char_reply(code));
}

void D2CSTcpSession::send_char_delete_result(bool success) {
    // result_code: 0x00 = success, 0x01 = failed
    const uint32_t code = success ? 0x00u : 0x01u;
    send_raw(protocol::d2cs::D2CSSessionFsm::make_delete_char_reply(code));
}

void D2CSTcpSession::send_ladder(
    const std::vector<domain::d2cs::LadderEntry>& /*entries*/) {
    // TODO(Phase4-Step5): implement LADDERREPLY encoder.
    // For now send a minimal stub: 3-byte header with zero entries.
    // Header: length(2 LE) + type(1)
    // LADDERREPLY = 0x11, total length = 3 (header only)
    std::vector<uint8_t> stub = {
        0x03, 0x00,  // length = 3 (LE)
        0x11         // type = LADDERREPLY
    };
    send_raw(std::move(stub));
}

} // namespace pvpgn::app::d2cs
