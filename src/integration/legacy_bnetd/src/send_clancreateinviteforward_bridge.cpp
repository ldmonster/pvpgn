// SPDX-License-Identifier: GPL-2.0-or-later
#include "integration/legacy_bnetd/send_clancreateinviteforward_bridge.hpp"

#include <cstdint>
#include <string>

#include "integration/legacy_bnetd/send_packet_bridge.hpp"
#include "protocol/bnet/codec.hpp"
#include "protocol/bnet/messages.hpp"
#include "protocol/common/writer.hpp"

extern "C" int pvpgn_v3_send_clancreateinviteforward(
    void*                conn_ptr,
    unsigned int         cookie,
    unsigned int         clan_tag,
    char const*          clan_name,
    char const*          clan_creator,
    char const* const*   member_names,
    unsigned int         member_count) noexcept {
    if (conn_ptr == nullptr) return 0;

    pvpgn::protocol::bnet::ClanCreateInviteForward m;
    m.cookie       = static_cast<std::uint32_t>(cookie);
    m.clan_tag     = static_cast<std::uint32_t>(clan_tag);
    m.clan_name    = clan_name    != nullptr ? clan_name    : "";
    m.clan_creator = clan_creator != nullptr ? clan_creator : "";

    if (member_names != nullptr) {
        for (unsigned int i = 0; i < member_count; ++i) {
            if (member_names[i] != nullptr)
                m.friend_names.emplace_back(member_names[i]);
            else
                m.friend_names.emplace_back();
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
