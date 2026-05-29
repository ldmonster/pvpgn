// SPDX-License-Identifier: GPL-2.0-or-later
#include "integration/legacy_bnetd/send_ladderreply_bridge.hpp"

#include <cstdint>
#include <cstring>

#include "integration/legacy_bnetd/send_packet_bridge.hpp"
#include "protocol/bnet/codec.hpp"
#include "protocol/bnet/messages.hpp"
#include "protocol/common/writer.hpp"

extern "C" int pvpgn_v3_send_ladderreply(
    void*                                      conn_ptr,
    unsigned int                               client_tag,
    unsigned int                               id,
    unsigned int                               type,
    unsigned int                               start,
    unsigned int                               count,
    struct pvpgn_v3_ladder_list_entry const*   entries) noexcept {
    if (conn_ptr == nullptr) return 0;

    pvpgn::protocol::bnet::LadderListReply m;
    m.client_tag = static_cast<std::uint32_t>(client_tag);
    m.id         = static_cast<std::uint32_t>(id);
    m.type       = static_cast<std::uint32_t>(type);
    m.start      = static_cast<std::uint32_t>(start);
    m.count      = static_cast<std::uint32_t>(count);

    if (count > 0u && entries != nullptr) {
        m.entries.reserve(count);
        for (unsigned int i = 0; i < count; ++i) {
            pvpgn::protocol::bnet::LadderListEntry e;
            e.current.wins       = entries[i].current.wins;
            e.current.loss       = entries[i].current.loss;
            e.current.disconnect = entries[i].current.disconnect;
            e.current.rating     = entries[i].current.rating;
            e.current.rank       = entries[i].current.rank;
            e.active.wins        = entries[i].active.wins;
            e.active.loss        = entries[i].active.loss;
            e.active.disconnect  = entries[i].active.disconnect;
            e.active.rating      = entries[i].active.rating;
            e.active.rank        = entries[i].active.rank;
            for (std::size_t t = 0; t < 6u; ++t)
                e.ttest[t] = entries[i].ttest[t];
            e.lastgame_current = entries[i].lastgame_current;
            e.lastgame_active  = entries[i].lastgame_active;
            if (entries[i].player_name != nullptr)
                e.player_name = entries[i].player_name;
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

    return ::pvpgn_v3_send_packet_try(
        conn_ptr,
        bytes.data(),
        static_cast<unsigned int>(bytes.size()));
}
