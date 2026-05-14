// SPDX-License-Identifier: GPL-2.0-or-later
//
// Strangler bridge: legacy `_client_anongame_profile` (FINDANONGAME
// sub-option 0x04) -> v3 typed reply.
//
// Resolves the requested account/clienttag/teams in legacy land,
// snapshots every value into `application::profile::ProfileInputs`,
// dispatches to the pure builder, then encodes + injects the resulting
// frame back into the legacy outqueue. Returns 1 on success and 0 on
// any fall-through (so the legacy handler runs as a safety net).

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "application/profile/profile_reply.hpp"
#include "integration/legacy_bnetd/dispatch.hpp"
#include "integration/legacy_bnetd/bridge_logger.hpp"
#include "protocol/bnet/anongame.hpp"
#include "protocol/bnet/codec.hpp"
#include "protocol/bnet/messages.hpp"
#include "protocol/common/writer.hpp"

#include "common/setup_before.h"
#include "common/packet.h"
#include "common/list.h"
#include "common/bnettime.h"
#include "common/bn_type.h"
#include "common/bnet_protocol.h"
#include "bnetd/account.h"
#include "bnetd/account_wrap.h"
#include "bnetd/connection.h"
#include "bnetd/team.h"
#include "common/setup_after.h"

// The bnetd headers nest their public API inside ``pvpgn::bnetd``.
// Inject those names into ``pvpgn`` so existing ``pvpgn::xxx``
// references in this bridge resolve via qualified-lookup through the
// using-directive (per [namespace.qual]).
namespace pvpgn { using namespace bnetd; }

namespace papp = pvpgn::application::profile;
namespace pb   = pvpgn::protocol::bnet;
namespace plb  = pvpgn::integration::legacy_bnetd;

namespace {

papp::LadderStats snap_ladder(pvpgn::t_account* acc,
                              pvpgn::t_clienttag ctag,
                              pvpgn::t_ladder_id id) {
    papp::LadderStats s;
    const int xp    = pvpgn::account_get_ladder_xp   (acc, ctag, id);
    const int level = pvpgn::account_get_ladder_level(acc, ctag, id);
    s.wins   = static_cast<std::uint16_t>(pvpgn::account_get_ladder_wins  (acc, ctag, id));
    s.losses = static_cast<std::uint16_t>(pvpgn::account_get_ladder_losses(acc, ctag, id));
    s.level  = static_cast<std::uint8_t> (level);
    s.calc   = static_cast<std::uint8_t> (pvpgn::account_get_profile_calcs(acc, xp, static_cast<unsigned>(level)));
    s.xp     = static_cast<std::uint16_t>(xp);
    s.rank   = static_cast<std::uint32_t>(pvpgn::account_get_ladder_rank  (acc, ctag, id));
    return s;
}

papp::RaceStats snap_race(pvpgn::t_account* acc,
                          pvpgn::t_clienttag ctag,
                          unsigned int race) {
    papp::RaceStats r;
    r.wins   = static_cast<std::uint16_t>(pvpgn::account_get_racewins  (acc, race, ctag));
    r.losses = static_cast<std::uint16_t>(pvpgn::account_get_racelosses(acc, race, ctag));
    return r;
}

}  // namespace

extern "C" int pvpgn_v3_profile_try(
    void* conn_ptr, void const* body, unsigned int body_size) {
    if (conn_ptr == nullptr || body == nullptr) return 0;

    // Body: [option=0x04][count u32 LE][username\0][clienttag is sometimes
    // appended by some clients but legacy never reads it]. Minimum 6 bytes
    // to hold option + count + 1-char username + NUL.
    if (body_size < 7) return 0;
    auto const* p = static_cast<const std::uint8_t*>(body);
    if (p[0] != 0x04) return 0;
    const std::uint32_t count =
        static_cast<std::uint32_t>(p[1])
        | (static_cast<std::uint32_t>(p[2]) << 8)
        | (static_cast<std::uint32_t>(p[3]) << 16)
        | (static_cast<std::uint32_t>(p[4]) << 24);

    // Locate the NUL-terminated username starting at p[5].
    std::size_t name_start = 5;
    std::size_t name_end   = name_start;
    while (name_end < body_size && p[name_end] != 0x00) ++name_end;
    if (name_end >= body_size) return 0;  // unterminated -> let legacy log it
    const std::size_t name_len = name_end - name_start;
    if (name_len == 0 || name_len > MAX_USERNAME_LEN) return 0;
    std::string username(
        reinterpret_cast<const char*>(p + name_start), name_len);

    auto* conn = static_cast<pvpgn::t_connection*>(conn_ptr);

    pvpgn::t_account* account =
        pvpgn::accountlist_find_account(username.c_str());
    if (account == nullptr) {
        // Match legacy: log + return -1 (we return 0 so legacy emits the
        // identical log path and answer).
        return 0;
    }

    // Resolve clienttag: prefer the account's online connection, fall
    // back to account_get_ll_clienttag (legacy-stored last-login tag).
    pvpgn::t_clienttag ctag = 0;
    if (auto* dest = pvpgn::connlist_find_connection_by_accountname(username.c_str())) {
        ctag = pvpgn::conn_get_clienttag(dest);
    } else {
        ctag = pvpgn::account_get_ll_clienttag(account);
    }
    if (ctag == 0) return 0;

    // Build the inputs snapshot.
    papp::ProfileInputs in;
    in.count        = count;
    in.profile_icon = pvpgn::account_icon_to_profile_icon(
        pvpgn::account_get_user_icon(account, ctag), account, ctag);

    in.solo = snap_ladder(account, ctag, pvpgn::ladder_id_solo);
    in.team = snap_ladder(account, ctag, pvpgn::ladder_id_team);
    in.ffa  = snap_ladder(account, ctag, pvpgn::ladder_id_ffa);

    pvpgn::t_list* teamlist = pvpgn::account_get_teams(account);
    in.has_stats = (in.solo.level > 0)
                || (in.team.level > 0)
                || (in.ffa.level > 0)
                || (teamlist != nullptr);

    if (in.has_stats) {
        in.random     = snap_race(account, ctag, W3_RACE_RANDOM);
        in.humans     = snap_race(account, ctag, W3_RACE_HUMANS);
        in.orcs       = snap_race(account, ctag, W3_RACE_ORCS);
        in.undead     = snap_race(account, ctag, W3_RACE_UNDEAD);
        in.nightelves = snap_race(account, ctag, W3_RACE_NIGHTELVES);
        in.demons     = snap_race(account, ctag, W3_RACE_DEMONS);

        if (teamlist != nullptr) {
            // Legacy lookup table (size 1..6 -> tag).
            const std::uint32_t teamtype[6] = {
                0u, 0x32565332u, 0x33565333u,
                0x34565334u, 0x35565335u, 0x36565336u};
            const unsigned int self_uid = pvpgn::account_get_uid(account);

            pvpgn::t_elem* curr = nullptr;
            LIST_TRAVERSE(teamlist, curr) {
                auto* team = static_cast<pvpgn::t_team*>(
                    pvpgn::elem_get_data(curr));
                if (team == nullptr) continue;
                if (pvpgn::team_get_clienttag(team) != ctag) continue;

                papp::ATTeamRecord rec{};
                const unsigned size = pvpgn::team_get_size(team);
                if (size == 0 || size > 6) continue;  // out-of-range guard
                rec.team_tag = teamtype[size - 1];
                const int xp    = pvpgn::team_get_xp(team);
                const int level = pvpgn::team_get_level(team);
                rec.wins   = static_cast<std::uint16_t>(pvpgn::team_get_wins  (team));
                rec.losses = static_cast<std::uint16_t>(pvpgn::team_get_losses(team));
                rec.level  = static_cast<std::uint8_t> (level);
                rec.calc   = static_cast<std::uint8_t> (
                    pvpgn::account_get_profile_calcs(account, xp, static_cast<unsigned>(level)));
                rec.xp     = static_cast<std::uint16_t>(xp);
                rec.rank   = static_cast<std::uint32_t>(pvpgn::team_get_rank(team));

                // Convert lastgame time -> 8-byte bn_long (LE).
                pvpgn::bn_long ltime{};
                pvpgn::t_bnettime bnt =
                    pvpgn::time_to_bnettime(pvpgn::team_get_lastgame(team), 0);
                pvpgn::bnettime_to_bn_long(bnt, &ltime);
                std::memcpy(rec.lastgame_bn_long.data(), &ltime, 8);

                rec.size_minus_one = static_cast<std::uint8_t>(size - 1);

                // Member names, excluding self (matches legacy loop).
                for (unsigned i = 0; i < size; ++i) {
                    if (pvpgn::team_get_memberuid(team, static_cast<int>(i)) == self_uid)
                        continue;
                    if (auto* m = pvpgn::team_get_member(team, static_cast<int>(i))) {
                        if (auto const* nm = pvpgn::account_get_name(m)) {
                            rec.other_members.emplace_back(nm);
                        }
                    }
                }
                in.teams.push_back(std::move(rec));
                if (in.teams.size() >= 16) break;
            }
        }
    }

    pb::AnonGameProfileReply reply = papp::build_profile_reply(in);
    auto env = pb::serialize_findanongame_reply(pb::AnonGameServer{reply});
    pvpgn::protocol::Writer w;
    if (auto st = pb::encode(w, env); !st) {
        std::string emsg = "encode failed: ";
        emsg += st.error().message();
        plb::bridge_log(pvpgn::core::LogLevel::Warn,
                        "v3_profile_bridge", emsg);
        return 0;
    }
    auto view = w.view();
    if (!plb::dispatch_bnet_frame_v3(conn_ptr, view.data(), view.size()))
        return 0;
    return 1;
}
