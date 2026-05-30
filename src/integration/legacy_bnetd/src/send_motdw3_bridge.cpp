// SPDX-License-Identifier: GPL-2.0-or-later
#include "integration/legacy_bnetd/send_motdw3_bridge.hpp"

#include <cstdint>

#include "integration/legacy_bnetd/send_packet_bridge.hpp"
#include "protocol/bnet/codec.hpp"
#include "protocol/bnet/messages.hpp"
#include "protocol/common/writer.hpp"

extern "C" int pvpgn_v3_send_motdw3(
    void*        conn_ptr,
    unsigned int msg_type,
    unsigned int curr_time,
    unsigned int first_news_time,
    unsigned int timestamp,
    unsigned int timestamp2,
    char const*  text) noexcept {
    if (conn_ptr == nullptr) return 0;

    pvpgn::protocol::bnet::MotdReply m;
    m.msg_type        = static_cast<std::uint8_t>(msg_type);
    m.curr_time       = static_cast<std::uint32_t>(curr_time);
    m.first_news_time = static_cast<std::uint32_t>(first_news_time);
    m.timestamp       = static_cast<std::uint32_t>(timestamp);
    m.timestamp2      = static_cast<std::uint32_t>(timestamp2);
    m.text            = (text != nullptr) ? text : "";

    pvpgn::protocol::Writer w;
    auto enc = pvpgn::protocol::bnet::encode(w, m);
    if (!enc.has_value()) return 0;

    auto bytes = w.take();
    if (bytes.empty()) return 0;
    if (bytes.size() > pvpgn::integration::legacy_bnetd::kSendPacketMaxSize) {
        return 0;
    }
    return ::pvpgn_v3_send_packet(
        conn_ptr,
        bytes.data(),
        static_cast<unsigned int>(bytes.size()));
}
