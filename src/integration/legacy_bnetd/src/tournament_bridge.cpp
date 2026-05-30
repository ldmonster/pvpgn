// SPDX-License-Identifier: GPL-2.0-or-later
//
// Strangler bridge: legacy `_client_anongame_tournament` (FINDANONGAME
// sub-option 0x07) -> v3 tournament-reply pipeline.
//
// Returns 1 on success, 0 on fall-through (legacy code must run).

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "application/tournament/tournament_reply.hpp"
#include "integration/legacy_bnetd/bridge_logger.hpp"
#include "protocol/bnet/anongame.hpp"
#include "protocol/bnet/codec.hpp"
#include "protocol/bnet/messages.hpp"
#include "protocol/common/writer.hpp"

#include "integration/legacy_bnetd/dispatch.hpp"

#include "common/setup_before.h"
#include "common/tag.h"
#include "bnetd/connection.h"
#include "bnetd/server.h"
#include "bnetd/tournament.h"
#include "common/setup_after.h"

namespace at  = pvpgn::application::tournament;
namespace pb  = pvpgn::protocol::bnet;
namespace plb = pvpgn::integration::legacy_bnetd;

namespace {

}  // namespace

extern "C" int pvpgn_v3_tournament(
    void* conn_ptr, void const* body, unsigned int body_size) {
    if (conn_ptr == nullptr || body == nullptr) return 0;
    if (body_size < 5) return 0;
    auto const* p = static_cast<const std::uint8_t*>(body);
    if (p[0] != 0x07) return 0;
    const std::uint32_t count =
        static_cast<std::uint32_t>(p[1])
        | (static_cast<std::uint32_t>(p[2]) << 8)
        | (static_cast<std::uint32_t>(p[3]) << 16)
        | (static_cast<std::uint32_t>(p[4]) << 24);

    auto* conn      = static_cast<pvpgn::bnetd::t_connection*>(conn_ptr);
    auto* account   = pvpgn::bnetd::conn_get_account(conn);
    if (account == nullptr) return 0;
    auto  clienttag = pvpgn::bnetd::conn_get_clienttag(conn);

    at::TournamentInputs in{};
    in.count             = count;
    in.now               = static_cast<std::uint32_t>(pvpgn::bnetd::now);
    in.start_preliminary = pvpgn::bnetd::tournament_get_start_preliminary();
    in.end_signup        = pvpgn::bnetd::tournament_get_end_signup();
    in.end_preliminary   = pvpgn::bnetd::tournament_get_end_preliminary();
    in.start_round_1     = pvpgn::bnetd::tournament_get_start_round_1();
    in.client_supported  =
        pvpgn::bnetd::tournament_check_client(clienttag) >= 0;
    in.signed_up         =
        pvpgn::bnetd::tournament_user_signed_up(account) >= 0;
    in.game_in_progress  =
        pvpgn::bnetd::tournament_get_game_in_progress() != 0;
    in.in_finals         =
        pvpgn::bnetd::tournament_get_in_finals_status(account) != 0;
    in.wins   = static_cast<std::uint8_t>(
        pvpgn::bnetd::tournament_get_stat(account, 1));
    in.losses = static_cast<std::uint8_t>(
        pvpgn::bnetd::tournament_get_stat(account, 2));
    in.ties   = static_cast<std::uint8_t>(
        pvpgn::bnetd::tournament_get_stat(account, 3));

    auto reply = at::build_tournament_reply(in);

    auto env = pb::serialize_findanongame_reply(pb::AnonGameServer{reply});
    pvpgn::protocol::Writer w;
    if (auto st = pb::encode(w, env); !st) {
        std::string emsg = "encode failed: ";
        emsg += st.error().message();
        plb::bridge_log(pvpgn::core::LogLevel::Warn,
                        "v3_tournament_bridge", emsg);
        return 0;
    }
    auto view = w.view();
    if (!plb::dispatch_bnet_frame_v3(conn_ptr, view.data(), view.size()))
        return 0;
    return 1;
}
