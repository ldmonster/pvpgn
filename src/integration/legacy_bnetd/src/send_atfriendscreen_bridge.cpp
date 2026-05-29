// SPDX-License-Identifier: GPL-2.0-or-later
#include "integration/legacy_bnetd/send_atfriendscreen_bridge.hpp"

#include <string>

#include "integration/legacy_bnetd/send_packet_bridge.hpp"
#include "protocol/bnet/codec.hpp"
#include "protocol/bnet/messages.hpp"
#include "protocol/common/writer.hpp"

extern "C" int pvpgn_v3_send_atfriendscreenreply(
    void*              conn_ptr,
    char const* const* names,
    unsigned int       count) noexcept {
    if (conn_ptr == nullptr) return 0;

    pvpgn::protocol::bnet::ArrangedTeamFriendScreenReply m;
    m.names.reserve(count);

    for (unsigned int i = 0; i < count; ++i) {
        m.names.emplace_back((names != nullptr && names[i] != nullptr) ? names[i] : "");
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
