// SPDX-License-Identifier: GPL-2.0-or-later
#include "integration/legacy_bnetd/send_realmlist_bridge.hpp"

#include <cstdint>

#include "integration/legacy_bnetd/send_packet_bridge.hpp"
#include "protocol/bnet/codec.hpp"
#include "protocol/bnet/messages.hpp"
#include "protocol/common/writer.hpp"

extern "C" int pvpgn_v3_send_realmlistreply(
    void*                             conn_ptr,
    struct pvpgn_v3_realm_entry const* entries,
    unsigned int                       count) noexcept {
    if (conn_ptr == nullptr) return 0;

    pvpgn::protocol::bnet::RealmListReply m;
    m.unknown1 = 0;
    if (entries != nullptr) {
        m.entries.reserve(count);
        for (unsigned int i = 0; i < count; ++i) {
            pvpgn::protocol::bnet::RealmListEntry e;
            e.unknown     = entries[i].unknown;
            e.name        = (entries[i].name        != nullptr) ? entries[i].name        : "";
            e.description = (entries[i].description != nullptr) ? entries[i].description : "";
            m.entries.push_back(std::move(e));
        }
    }

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
