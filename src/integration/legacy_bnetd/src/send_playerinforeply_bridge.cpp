// SPDX-License-Identifier: GPL-2.0-or-later
#include "integration/legacy_bnetd/send_playerinforeply_bridge.hpp"

#include <cstdint>
#include <cstring>

#include "integration/legacy_bnetd/send_packet_bridge.hpp"
#include "protocol/common/writer.hpp"

namespace {
// SERVER_PLAYERINFOREPLY (SID_USERDATA) = 0x0A, class bnet (0xFF).
constexpr std::uint8_t kServerPlayerInfoReplyCode = 0x0a;
}  // namespace

extern "C" int pvpgn_v3_send_playerinforeply(void*       conn_ptr,
                                              char const* account_name,
                                              char const* player_info,
                                              char const* username) noexcept {
    if (conn_ptr == nullptr) return 0;
    if (account_name == nullptr) return 0;
    if (player_info == nullptr) return 0;
    if (username == nullptr) return 0;

    pvpgn::protocol::Writer w;
    w.begin_bnet_packet(kServerPlayerInfoReplyCode);
    w.write_cstring(account_name);
    w.write_cstring(player_info);
    w.write_cstring(username);
    auto fin = w.finalize_bnet_packet();
    if (!fin.has_value()) return 0;

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
