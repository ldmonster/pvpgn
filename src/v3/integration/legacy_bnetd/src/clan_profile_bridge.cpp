// SPDX-License-Identifier: GPL-2.0-or-later
//
// Strangler bridge: legacy `_client_anongame_profile_clan` (FINDANONGAME
// sub-option 0x08) -> v3 typed reply.
//
// The legacy handler is effectively a stub (the clan-stats payload is
// commented out and the `clan` lookup result is unused). This bridge
// reproduces the exact stub bytes via the typed protocol layer so the
// codec golden tests own the wire format.
//
// Returns 1 on success, 0 on fall-through.

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "integration/legacy_bnetd/dispatch.hpp"
#include "integration/legacy_bnetd/bridge_logger.hpp"
#include "protocol/bnet/anongame.hpp"
#include "protocol/bnet/codec.hpp"
#include "protocol/bnet/messages.hpp"
#include "protocol/common/writer.hpp"

#include "common/setup_before.h"
#include "common/setup_after.h"

namespace pb = pvpgn::protocol::bnet;
namespace plb = pvpgn::integration::legacy_bnetd;

extern "C" int pvpgn_v3_clan_profile_try(
    void* conn_ptr, void const* body, unsigned int body_size) {
    if (conn_ptr == nullptr || body == nullptr) return 0;
    // Body: [option=0x08][count u32 LE][clantag u32][clienttag u32] = 13 bytes
    if (body_size < 13) return 0;
    auto const* p = static_cast<const std::uint8_t*>(body);
    if (p[0] != 0x08) return 0;
    const std::uint32_t count =
        static_cast<std::uint32_t>(p[1])
        | (static_cast<std::uint32_t>(p[2]) << 8)
        | (static_cast<std::uint32_t>(p[3]) << 16)
        | (static_cast<std::uint32_t>(p[4]) << 24);

    pb::AnonGameClanProfileReply reply{};
    reply.count    = count;
    reply.rescount = 0;
    reply.trailer  = {0x00};  // matches the legacy single zero-byte trailer

    auto env = pb::serialize_findanongame_reply(pb::AnonGameServer{reply});
    pvpgn::protocol::Writer w;
    if (auto st = pb::encode(w, env); !st) {
        std::string msg = "encode failed: ";
        msg += st.error().message();
        plb::bridge_log(pvpgn::core::LogLevel::Warn,
                        "v3_clan_profile_bridge", msg);
        return 0;
    }
    auto view = w.view();
    if (!plb::dispatch_bnet_frame_v3(conn_ptr, view.data(), view.size()))
        return 0;
    return 1;
}
