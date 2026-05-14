// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file tournament_reply.hpp
/// Pure builder for the FINDANONGAME 0x07 (TOURNAMENT) reply.
///
/// Mirrors the legacy state machine in
/// `src/bnetd/handle_anongame.cpp::_client_anongame_tournament` but
/// accepts every input as a value so the logic can be exercised in
/// isolation. Produces a `protocol::bnet::AnonGameTournamentReply`
/// directly so the caller only has to wrap + serialise.

#include <cstdint>

#include "protocol/bnet/anongame.hpp"

namespace pvpgn::application::tournament {

/// Snapshot of every legacy global the tournament-reply state machine
/// reads. All times are seconds-since-epoch in the same domain that
/// the legacy `now` global uses (also passed in here).
struct TournamentInputs {
    std::uint32_t count             = 0;  // echo from request
    std::uint32_t now               = 0;
    std::uint32_t start_preliminary = 0;  // 0 -> tournament not configured
    std::uint32_t end_signup        = 0;
    std::uint32_t end_preliminary   = 0;
    std::uint32_t start_round_1     = 0;
    bool          client_supported  = false;  // tournament_check_client>=0
    bool          signed_up         = false;  // tournament_user_signed_up>=0
    bool          game_in_progress  = false;
    bool          in_finals         = false;
    std::uint8_t  wins              = 0;
    std::uint8_t  losses            = 0;
    std::uint8_t  ties              = 0;
};

/// Convert a wall-clock time-since-epoch into the magic packed value
/// the WC3 client expects (extracted verbatim from
/// `_tournament_time_convert`).
std::uint32_t convert_time(std::uint32_t time);

/// Build the tournament reply from the supplied inputs. Always
/// produces a valid reply (even type 0 / "no tournament").
protocol::bnet::AnonGameTournamentReply build_tournament_reply(
    const TournamentInputs& in);

}  // namespace pvpgn::application::tournament
