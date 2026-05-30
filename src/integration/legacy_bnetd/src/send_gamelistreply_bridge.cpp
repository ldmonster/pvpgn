// SPDX-License-Identifier: GPL-2.0-or-later
#include "integration/legacy_bnetd/send_gamelistreply_bridge.hpp"

#include <cstdint>
#include <cstring>

#include "integration/legacy_bnetd/send_packet_bridge.hpp"
#include "protocol/bnet/codec.hpp"
#include "protocol/bnet/messages.hpp"
#include "protocol/common/writer.hpp"

extern "C" int pvpgn_v3_send_gamelistreply(
    void*                                    conn_ptr,
    std::uint32_t                            sstatus,
    struct pvpgn_v3_game_list_entry const*   entries,
    unsigned int                             count) noexcept {
    if (conn_ptr == nullptr) return 0;

    pvpgn::protocol::bnet::GameListReply m;
    m.sstatus = sstatus;

    if (count > 0u && entries != nullptr) {
        m.entries.reserve(count);
        for (unsigned int i = 0; i < count; ++i) {
            pvpgn::protocol::bnet::GameListEntry e;
            e.gametype  = entries[i].gametype;
            e.unknown1  = entries[i].unknown1;
            e.unknown3  = entries[i].unknown3;
            e.port      = entries[i].port;
            e.game_ip   = entries[i].game_ip;
            e.unknown4  = entries[i].unknown4;
            e.unknown5  = entries[i].unknown5;
            e.status    = entries[i].status;
            e.unknown6  = entries[i].unknown6;
            if (entries[i].game_name != nullptr) e.game_name = entries[i].game_name;
            if (entries[i].password  != nullptr) e.password  = entries[i].password;
            if (entries[i].info      != nullptr) e.info      = entries[i].info;
            m.entries.push_back(std::move(e));
        }
    }

    pvpgn::protocol::Writer w;
    auto enc = pvpgn::protocol::bnet::encode(w, m);
    if (!enc.has_value()) return 0;

    auto bytes = w.take();
    if (bytes.empty()) return 0;
    if (bytes.size() > pvpgn::integration::legacy_bnetd::kSendPacketMaxSize)
        return 0;

    return ::pvpgn_v3_send_packet(
        conn_ptr,
        bytes.data(),
        static_cast<unsigned int>(bytes.size()));
}
