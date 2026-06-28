// SPDX-License-Identifier: GPL-2.0-or-later

/// @file d2dbs_tcp_session.cpp
/// Implementation of `D2DBSTcpSession` — the per-connection D2DBS stack.

#include "app/d2dbs/d2dbs_tcp_session.hpp"

#include "infra/net/tcp_session.hpp"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <utility>
#include <vector>

#include <boost/asio/error.hpp>
#include <boost/system/error_code.hpp>

#include "core/bytes.hpp"
#include "protocol/d2dbs/codec.hpp"
#include "protocol/d2dbs/fsm.hpp"
#include "app/d2dbs/legacy_d2dbs_bridges/d2dbs_prefs_bridge.hpp"

namespace pvpgn::app::d2dbs {

// ---------------------------------------------------------------------------
// Constructor — build the handler, then wrap its callbacks so the session can
// capture per-request context (seqno/datatype/char_name) for the replies.
// ---------------------------------------------------------------------------

D2DBSTcpSession::D2DBSTcpSession(std::shared_ptr<infra::net::TcpSession> tcp)
    : tcp_(std::move(tcp))
    , save_repo_{}
    , ladder_repo_{}
    , handler_(std::make_unique<D2DBSSessionHandler>(
          save_repo_, ladder_repo_, *this))
{
    using namespace protocol::d2dbs;
    D2DBSFsmCallbacks hc = handler_->make_callbacks();

    D2DBSFsmCallbacks fc;
    fc.on_char_save = [this, h = hc.on_char_save](const D2DBSCharSaveData& r) {
        cur_seqno_     = r.seqno;
        cur_datatype_  = static_cast<std::uint16_t>(r.datatype);
        cur_char_name_ = r.char_name;
        return h ? h(r) : core::Result<void, core::Error>();
    };
    fc.on_char_load = [this, h = hc.on_char_load](const D2DBSCharLoadData& r) {
        cur_seqno_     = r.seqno;
        cur_datatype_  = static_cast<std::uint16_t>(r.datatype);
        cur_char_name_ = r.char_name;
        return h ? h(r) : core::Result<void, core::Error>();
    };
    fc.on_char_lock = [this, h = hc.on_char_lock](const D2DBSCharLockReq& r) {
        cur_seqno_     = r.seqno;
        cur_char_name_ = r.char_name;
        return h ? h(r) : core::Result<void, core::Error>();
    };
    fc.on_char_ladder = std::move(hc.on_char_ladder);
    fc.on_echo_reply  = std::move(hc.on_echo_reply);
    fc.on_disconnect  = std::move(hc.on_disconnect);

    fsm_ = std::make_unique<D2DBSSessionFsm>(std::move(fc));
}

// ---------------------------------------------------------------------------
// start()
// ---------------------------------------------------------------------------

void D2DBSTcpSession::start() {
    auto self = shared_from_this();

    tcp_->set_on_bytes([self](core::ByteView bv) {
        const auto* data = reinterpret_cast<const std::uint8_t*>(bv.data());
        std::size_t size = bv.size();

        // The D2GS->D2DBS connection opens with a single connect-class byte
        // (kConnectClassD2gsToD2dbs = 0x65) before any framed packet; consume
        // it once. Anything else is a bad connection class -> drop.
        if (!self->init_consumed_) {
            if (size == 0) return;
            if (data[0] != protocol::d2dbs::kConnectClassD2gsToD2dbs) {
                std::cerr << "[d2dbs] bad connect-class byte: "
                          << static_cast<int>(data[0]) << "\n";
                self->tcp_->close();
                return;
            }
            self->init_consumed_ = true;
            ++data;
            --size;
            if (size == 0) return;
        }

        auto result = self->fsm_->feed(data, size);
        if (!result) {
            std::cerr << "[d2dbs] session error: "
                      << result.error().message() << "\n";
            self->tcp_->close();
        }
    });

    tcp_->set_on_close([self](const boost::system::error_code& ec) {
        if (ec && ec != boost::asio::error::eof) {
            std::cerr << "[d2dbs] session closed: " << ec.message() << "\n";
        }
    });

    tcp_->start();
}

// ---------------------------------------------------------------------------
// send_raw()
// ---------------------------------------------------------------------------

void D2DBSTcpSession::send_raw(std::vector<std::uint8_t> bytes) {
    if (!tcp_) return;
    std::vector<std::byte> buf;
    buf.reserve(bytes.size());
    for (auto b : bytes) buf.push_back(static_cast<std::byte>(b));
    tcp_->send(std::move(buf));
}

// ---------------------------------------------------------------------------
// ID2DBSSessionEgress
// ---------------------------------------------------------------------------

void D2DBSTcpSession::send_char_save_result(bool success) {
    send_raw(protocol::d2dbs::D2DBSSessionFsm::make_save_data_reply(
        cur_seqno_,
        success ? protocol::d2dbs::kSaveDataSuccess
                : protocol::d2dbs::kSaveDataFailed,
        cur_datatype_, cur_char_name_));
}

void D2DBSTcpSession::send_char_load_result(
    bool success, const domain::d2dbs::CharacterSaveData* data) {
    const std::uint32_t result = (success && data)
        ? protocol::d2dbs::kGetDataSuccess
        : protocol::d2dbs::kGetDataFailed;
    const std::uint32_t creattime = (success && data) ? data->timestamp : 0u;
    const std::vector<std::uint8_t> blob =
        (success && data) ? data->data : std::vector<std::uint8_t>{};
    // allowladder mirrors the oracle (dbspacket.cpp dbs_packet_getdata): on a
    // successful load it is 1 iff the character's create_time >= the configured
    // ladderinit_time, else 0; on failure it stays 0. With the default
    // ladderinit_time = 0 every loaded character is ladder-eligible (matching
    // the original). The previous hardcoded 0 reported every char as
    // ladder-ineligible to the D2GS.
    const std::uint32_t allowladder =
        (success && data &&
         creattime >= pvpgn_v3_d2dbs_prefs_get_ladderinit_time())
            ? 1u : 0u;
    send_raw(protocol::d2dbs::D2DBSSessionFsm::make_get_data_reply(
        cur_seqno_, result, creattime, allowladder,
        cur_datatype_, cur_char_name_, blob));
}

// CHAR_LOCK (login/logout) and UPDATE_LADDER are fire-and-forget in the
// original (no reply packet), so these acks produce no wire output.
void D2DBSTcpSession::send_char_login_result(bool /*success*/) {}
void D2DBSTcpSession::send_char_logout_result(bool /*success*/) {}
void D2DBSTcpSession::send_ladder_update_result(bool /*success*/) {}

}  // namespace pvpgn::app::d2dbs
