// SPDX-License-Identifier: GPL-2.0-or-later
#include "integration/legacy_bnetd/send_realmjoin_bridge.hpp"

#include <cstdint>

#include "integration/legacy_bnetd/send_packet_bridge.hpp"
#include "protocol/bnet/codec.hpp"
#include "protocol/bnet/messages.hpp"
#include "protocol/common/writer.hpp"

extern "C" int pvpgn_v3_send_realmjoinreply(
    void*                conn_ptr,
    unsigned int         seqno,
    unsigned int         u1,
    unsigned int         bncs_addr1,
    unsigned int         session_num,
    unsigned int         addr,
    unsigned int         port,
    unsigned int         u3,
    unsigned int         session_key,
    unsigned int         u5,
    unsigned int         u6,
    unsigned int         client_tag,
    unsigned int         version_id,
    unsigned int         bncs_addr2,
    unsigned int         u7,
    unsigned int const*  secret_hash,
    char const*          account_name) noexcept {
    if (conn_ptr == nullptr) return 0;

    pvpgn::protocol::bnet::RealmJoinReply m;
    m.seqno       = static_cast<std::uint32_t>(seqno);
    m.u1          = static_cast<std::uint32_t>(u1);
    m.bncs_addr1  = static_cast<std::uint32_t>(bncs_addr1);
    m.session_num = static_cast<std::uint32_t>(session_num);
    m.addr        = static_cast<std::uint32_t>(addr);
    m.port        = static_cast<std::uint16_t>(port);
    m.u3          = static_cast<std::uint16_t>(u3);
    m.session_key = static_cast<std::uint32_t>(session_key);
    m.u5          = static_cast<std::uint32_t>(u5);
    m.u6          = static_cast<std::uint32_t>(u6);
    m.client_tag  = static_cast<std::uint32_t>(client_tag);
    m.version_id  = static_cast<std::uint32_t>(version_id);
    m.bncs_addr2  = static_cast<std::uint32_t>(bncs_addr2);
    m.u7          = static_cast<std::uint32_t>(u7);
    if (secret_hash != nullptr) {
        for (int i = 0; i < 5; ++i)
            m.secret_hash[static_cast<std::size_t>(i)] =
                static_cast<std::uint32_t>(secret_hash[i]);
    }
    m.account_name = (account_name != nullptr) ? account_name : "";

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
