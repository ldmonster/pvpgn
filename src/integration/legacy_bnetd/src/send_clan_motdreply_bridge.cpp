// SPDX-License-Identifier: GPL-2.0-or-later
#include "integration/legacy_bnetd/send_clan_motdreply_bridge.hpp"

#include <cstdint>
#include <string>

#include "integration/legacy_bnetd/send_packet_bridge.hpp"
#include "protocol/bnet/codec.hpp"
#include "protocol/bnet/messages.hpp"
#include "protocol/common/writer.hpp"

extern "C" int pvpgn_v3_send_clan_motdreply(void*        conn_ptr,
                                             unsigned int cookie,
                                             unsigned int unknown1,
                                             char const*  motd) noexcept {
    if (conn_ptr == nullptr) return 0;

    pvpgn::protocol::bnet::ClanMotdReply m;
    m.cookie   = static_cast<std::uint32_t>(cookie);
    m.unknown1 = static_cast<std::uint32_t>(unknown1);
    m.motd     = (motd != nullptr) ? std::string{motd} : std::string{};

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
