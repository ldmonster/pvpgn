// SPDX-License-Identifier: GPL-2.0-or-later
#include "integration/legacy_bnetd/send_friendslist_bridge.hpp"

#include <cstdint>
#include <string>

#include "integration/legacy_bnetd/send_packet_bridge.hpp"
#include "protocol/bnet/codec.hpp"
#include "protocol/bnet/messages.hpp"
#include "protocol/common/writer.hpp"

extern "C" int pvpgn_v3_send_friendslistreply(
    void*                              conn_ptr,
    struct pvpgn_v3_friend_entry const* entries,
    unsigned int                        count) noexcept {
    if (conn_ptr == nullptr) return 0;

    pvpgn::protocol::bnet::FriendsListReply m;
    m.entries.reserve(count);

    for (unsigned int i = 0; i < count; ++i) {
        pvpgn::protocol::bnet::FriendsListEntry e;
        e.name          = (entries[i].username      != nullptr) ? entries[i].username      : "";
        e.status        = entries[i].status;
        e.location      = entries[i].location;
        e.client_tag    = static_cast<std::uint32_t>(entries[i].client_tag);
        e.location_name = (entries[i].location_name != nullptr) ? entries[i].location_name : "";
        m.entries.push_back(std::move(e));
    }

    pvpgn::protocol::Writer w;
    auto enc = pvpgn::protocol::bnet::encode(w, m);
    if (!enc.has_value()) return 0;

    auto bytes = w.take();
    if (bytes.empty()) return 0;
    if (bytes.size() > pvpgn::integration::legacy_bnetd::kSendPacketMaxSize) {
        return 0;
    }
    return ::pvpgn_v3_send_packet_try(
        conn_ptr,
        bytes.data(),
        static_cast<unsigned int>(bytes.size()));
}
