// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file profile_reply.hpp
/// Pure builder for the FINDANONGAME 0x04 (PROFILE) reply.
///
/// Mirrors the legacy state machine in
/// `src/bnetd/handle_anongame.cpp::_client_anongame_profile`. Every
/// account/team lookup is hoisted into a value-typed `ProfileInputs`
/// struct so the reply assembly is testable in isolation.
///
/// Output is a `protocol::bnet::AnonGameProfileReply` whose typed
/// header is filled in directly while the variable-length WAR3 stats
/// payload (ladder sections, race header + 6x race wins/losses, AT
/// team list) is materialised into the opaque `data` blob byte-for-byte
/// the same way legacy emits it.

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "protocol/bnet/anongame.hpp"

namespace pvpgn::application::profile {

/// Per-ladder stats snapshot (solo / team / ffa).
struct LadderStats {
    std::uint16_t wins   = 0;
    std::uint16_t losses = 0;
    std::uint8_t  level  = 0;
    std::uint8_t  calc   = 0;   // account_get_profile_calcs(xp, level)
    std::uint16_t xp     = 0;
    std::uint32_t rank   = 0;
};

/// Per-race wins/losses snapshot.
struct RaceStats {
    std::uint16_t wins   = 0;
    std::uint16_t losses = 0;
};

/// One AT (arranged-team) record.
///
/// The `lastgame_bn_long` field is the 8 raw little-endian bytes of the
/// legacy `bnettime_to_bn_long` conversion result. We expose it as raw
/// bytes so the bridge can call into legacy time helpers and the pure
/// builder stays free of any time conversion logic.
struct ATTeamRecord {
    /// One of legacy `teamtype[size]` (e.g. 0x32565332 for size=2).
    std::uint32_t                     team_tag        = 0;
    std::uint16_t                     wins            = 0;
    std::uint16_t                     losses          = 0;
    std::uint8_t                      level           = 0;
    std::uint8_t                      calc            = 0;
    std::uint16_t                     xp              = 0;
    std::uint32_t                     rank            = 0;
    /// Raw 8 bytes of `bnettime_to_bn_long(lastgame)`; written verbatim.
    std::array<std::uint8_t, 8>       lastgame_bn_long{};
    /// `team_get_size(team) - 1`.
    std::uint8_t                      size_minus_one  = 0;
    /// Names of the *other* members (i.e. excluding the requesting
    /// account). Each is appended NUL-terminated, in legacy order.
    std::vector<std::string>          other_members;
};

/// All inputs the PROFILE reply needs.
struct ProfileInputs {
    std::uint32_t  count        = 0;  // echo from request
    std::uint32_t  profile_icon = 0;  // already resolved by caller
    /// True if the account has any WAR3 stats (any of solo/team/ffa
    /// level > 0 OR account_get_teams != null).
    bool           has_stats    = false;

    LadderStats    solo;
    LadderStats    team;
    LadderStats    ffa;

    RaceStats      random;
    RaceStats      humans;
    RaceStats      orcs;
    RaceStats      undead;
    RaceStats      nightelves;
    RaceStats      demons;

    /// Up to 16 AT teams (legacy caps the loop at 16).
    std::vector<ATTeamRecord>  teams;
};

/// Build the PROFILE reply from snapshot inputs. Always succeeds.
protocol::bnet::AnonGameProfileReply build_profile_reply(
    const ProfileInputs& in);

}  // namespace pvpgn::application::profile
