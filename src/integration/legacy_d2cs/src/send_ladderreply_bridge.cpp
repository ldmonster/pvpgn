// SPDX-License-Identifier: GPL-2.0-or-later
#include "integration/legacy_d2cs/send_ladderreply_bridge.hpp"

#include <cstring>
#include <vector>

#include "integration/legacy_d2cs/send_packet_bridge.hpp"
#include "protocol/d2cs/ladderreply_encoder.hpp"

namespace lr = pvpgn::protocol::d2cs::ladderreply;

extern "C" int pvpgn_v3_d2cs_send_ladderreply(
    void*                            conn_ptr,
    unsigned int                     type,
    unsigned int                     start_pos,
    pvpgn_v3_d2cs_ladder_entry const* entries,
    unsigned int                     count) noexcept {
    if (conn_ptr == nullptr) return 0;
    if (count > 0 && entries == nullptr) return 0;
    if (type > 0xFFu) return 0;
    if (start_pos > 0xFFFFu) return 0;

    std::vector<lr::LadderInfo> v;
    v.reserve(count);
    for (unsigned int i = 0; i < count; ++i) {
        lr::LadderInfo li{};
        li.exp_low  = entries[i].exp_low;
        li.exp_high = entries[i].exp_high;
        li.status   = entries[i].status;
        li.level    = entries[i].level;
        li.u1       = entries[i].u1;
        std::memcpy(li.charname.data(), entries[i].charname,
                    li.charname.size());
        v.push_back(li);
    }

    auto packets = lr::encode(static_cast<std::uint8_t>(type),
                              static_cast<std::uint16_t>(start_pos),
                              v);

    for (const auto& p : packets) {
        if (p.bytes.size() >
            pvpgn::integration::legacy_d2cs::kSendPacketMaxSize) {
            return 0;
        }
        const int rc = ::pvpgn_v3_d2cs_send_packet_try(
            conn_ptr, p.bytes.data(),
            static_cast<unsigned int>(p.bytes.size()));
        if (rc != 1) return 0;
    }
    return 1;
}
