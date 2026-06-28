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

namespace {

// Shared realm secret used to derive/validate the d2cs LOGINREQ token
// (blizzard_hash(key ‖ account ‖ sessionnum ‖ seqno)). A realm-join issues the
// token with this key; the d2cs validates it here. Mirrors the original's
// session-bound auth without a live bnetd link. Mock clients use the same key.
constexpr const char* kRealmKey = "pvpgn-v3-d2cs-realm-secret-v1";

// Portrait constants mirror the legacy d2cs encoding (d2charfile.cpp /
// d2cs_d2gs_character.h):
//   header = 0x8084 (LE bytes 0x84 0x80), gfx/color/u2 pad = 0xFF,
//   u1 = 0x80 (MASK), status carries the 0x80 MASK bit, chclass = class+1,
//   ladder = 1 (ladder) or 0xFF (pad).
constexpr uint8_t kPortraitPadByte = 0xFF;
constexpr uint8_t kPortraitMask    = 0x80;

// Build the 33 wire bytes of a character's portrait block. The legacy server
// appends the portrait with `packet_append_string`, i.e. the bytes up to (but
// not including) the struct's trailing `end` (0x00) byte, followed by a NUL.
// The `charlistreply` encoder appends that trailing NUL itself, so we emit the
// 33 leading bytes here.
std::vector<std::byte> build_portrait(const domain::d2cs::CharacterInfo& c) {
    std::vector<std::byte> p;
    p.reserve(33);
    auto push = [&p](uint8_t b) { p.push_back(static_cast<std::byte>(b)); };

    push(0x84);                               // header low
    push(0x80);                               // header high
    for (int i = 0; i < 11; ++i) push(kPortraitPadByte);  // gfx[11]
    push(static_cast<uint8_t>(static_cast<uint8_t>(c.class_) + 1));  // chclass
    for (int i = 0; i < 11; ++i) push(kPortraitPadByte);  // color[11]
    push(c.level);                            // level
    push(static_cast<uint8_t>(static_cast<uint8_t>(c.flags) | kPortraitMask));  // status
    for (int i = 0; i < 3; ++i) push(kPortraitMask);      // u1[3]
    const bool is_ladder =
        domain::d2cs::has_flag(c.flags, domain::d2cs::CharacterFlags::Ladder);
    push(is_ladder ? 0x01 : kPortraitPadByte); // ladder
    for (int i = 0; i < 2; ++i) push(kPortraitPadByte);   // u2[2]
    // The trailing `end` (0x00) byte is supplied by the encoder's NUL.
    return p;
}

}  // namespace

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

D2CSTcpSession::D2CSTcpSession(std::shared_ptr<infra::net::TcpSession> tcp)
    : tcp_(std::move(tcp))
    , char_repo_{}
    , ladder_repo_{}
    , handler_(std::make_unique<D2CSSessionHandler>(
          char_repo_, ladder_repo_, *this, std::string{kRealmKey}))
    , fsm_(std::make_unique<protocol::d2cs::D2CSSessionFsm>(
          handler_->make_callbacks()))
{}

// ---------------------------------------------------------------------------
// start()
// ---------------------------------------------------------------------------

void D2CSTcpSession::start() {
    auto self = shared_from_this();

    tcp_->set_on_bytes([self](core::ByteView bv) {
        const auto* data = reinterpret_cast<const uint8_t*>(bv.data());
        size_t      size = bv.size();

        // The connection opens with a single init class byte
        // (CLIENT_INITCONN_CLASS_D2CS = 0x01) before any framed packet, like
        // the BNCS/BNFTP listeners. Consume it once; anything else is a bad
        // connection class and the original drops the connection.
        if (!self->init_consumed_) {
            if (size == 0) return;            // wait for the byte
            if (data[0] != 0x01) {
                std::cerr << "[d2cs] bad init class byte: "
                          << static_cast<int>(data[0]) << "\n";
                self->tcp_->close();
                return;
            }
            self->init_consumed_ = true;
            ++data;
            --size;
            if (size == 0) return;            // init byte arrived alone
        }

        // Feed the remaining raw bytes into the FSM reassembly buffer.
        auto result = self->fsm_->feed(data, size);
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

void D2CSTcpSession::send_motd(std::string_view message) {
    send_raw(protocol::d2cs::D2CSSessionFsm::make_motd_reply(message));
}

// Default per-account character cap (legacy d2cs prefs_get_maxchar default).
static constexpr uint16_t kDefaultMaxChar = 8;

void D2CSTcpSession::send_char_list(
    const std::vector<domain::d2cs::CharacterInfo>& chars) {
    // Build the per-character entries (name + portrait block) for the
    // wire-accurate CHARLISTREPLY encoder.
    std::vector<protocol::d2cs::charlistreply::CharEntry> entries;
    entries.reserve(chars.size());
    for (const auto& c : chars) {
        protocol::d2cs::charlistreply::CharEntry e;
        e.charname = c.name;
        e.portrait = build_portrait(c);
        entries.push_back(std::move(e));
    }

    // `maxchar` doubles as the "new char allowed" signal: report the cap only
    // when there is still room, otherwise 0 (so the client disables Create).
    const uint16_t maxchar_field =
        (chars.size() < kDefaultMaxChar) ? kDefaultMaxChar : 0;

    send_raw(protocol::d2cs::D2CSSessionFsm::make_char_list_reply(
        maxchar_field, entries));
}

void D2CSTcpSession::send_char_list_result(bool success) {
    // Send an empty char list on failure (client will show "no characters").
    if (!success) {
        send_raw(protocol::d2cs::D2CSSessionFsm::make_char_list_reply(
            kDefaultMaxChar, {}));
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

void D2CSTcpSession::send_char_create_result(
    domain::d2cs::CharacterCreateResult result) {
    // Map to the original CREATECHARREPLY wire codes: SUCCEED 0x00,
    // ALREADY_EXIST 0x14 (bad name OR duplicate), FAILED 0x01.
    uint32_t code = 0x01u;  // Failed
    switch (result) {
        case domain::d2cs::CharacterCreateResult::Succeed:  code = 0x00u; break;
        case domain::d2cs::CharacterCreateResult::Rejected: code = 0x14u; break;
        case domain::d2cs::CharacterCreateResult::Failed:   code = 0x01u; break;
    }
    send_raw(protocol::d2cs::D2CSSessionFsm::make_create_char_reply(code));
}

void D2CSTcpSession::send_char_delete_result(bool success) {
    // result_code: 0x00 = success, 0x01 = failed
    const uint32_t code = success ? 0x00u : 0x01u;
    send_raw(protocol::d2cs::D2CSSessionFsm::make_delete_char_reply(code));
}

void D2CSTcpSession::send_ladder(
    const std::vector<domain::d2cs::LadderEntry>& /*entries*/) {
    // TODO: implement LADDERREPLY encoder.
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
