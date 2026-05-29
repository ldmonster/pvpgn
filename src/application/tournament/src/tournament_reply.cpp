// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/tournament/tournament_reply.hpp"

namespace pvpgn::application::tournament {

namespace pb = pvpgn::protocol::bnet;

std::uint32_t convert_time(std::uint32_t time) {
    // Verbatim from legacy `_tournament_time_convert`.
    std::uint32_t tmp1 = time - 1059179400u;  // 0x3F21CB88
    std::uint32_t tmp2 = static_cast<std::uint32_t>(
        static_cast<double>(tmp1) * 0.59604645);
    std::uint32_t tmp3 = tmp2 + 3276999960u;
    return tmp3;
}

namespace {

// All replies share these zero-init fields; most cases only override a
// handful. We start from a fully-zeroed reply and patch.
pb::AnonGameTournamentReply make_base(std::uint32_t count) {
    pb::AnonGameTournamentReply r{};
    r.count = count;
    return r;
}

}  // namespace

pb::AnonGameTournamentReply build_tournament_reply(const TournamentInputs& in) {
    auto r = make_base(in.count);

    // Type 0: no tournament for this user/client.
    if (in.start_preliminary == 0
        || (in.end_signup <= in.now && !in.signed_up)
        || !in.client_supported) {
        // All fields default-zero -> type 0.
        return r;
    }

    if (in.start_preliminary >= in.now) {
        // Type 1: notice (countdown to prelim start).
        r.type      = 1;
        r.unknown4  = 0x0000;
        r.timestamp = convert_time(in.start_preliminary);
        r.unknown5  = 0x01;
        r.countdown = static_cast<std::uint16_t>(
            in.start_preliminary - in.now);
        r.unknown3  = 0x00;
        r.selection = 2;
        return r;
    }

    if (in.end_signup >= in.now) {
        // Type 2: signups open, play active.
        r.type      = 2;
        r.unknown4  = 0x0828;
        r.timestamp = convert_time(in.end_signup);
        r.unknown5  = 0x01;
        r.countdown = static_cast<std::uint16_t>(
            in.end_signup - in.now);
        r.wins      = in.wins;
        r.losses    = in.losses;
        r.ties      = in.ties;
        r.unknown3  = 0x08;
        r.selection = 2;
        return r;
    }

    if (in.end_preliminary >= in.now) {
        // Type 3: prelim period.
        r.type      = 3;
        r.unknown4  = 0x0828;
        r.timestamp = convert_time(in.end_preliminary);
        r.unknown5  = 0x01;
        r.countdown = static_cast<std::uint16_t>(
            in.end_preliminary - in.now);
        r.wins      = in.wins;
        r.losses    = in.losses;
        r.ties      = in.ties;
        r.unknown3  = 0x08;
        r.selection = 2;
        return r;
    }

    if (in.start_round_1 >= in.now && in.game_in_progress) {
        // Type 4: prelim over, finals not yet started.
        r.type      = 4;
        r.unknown4  = 0x0000;
        r.timestamp = convert_time(in.start_round_1);
        r.unknown5  = 0x01;
        r.countdown = static_cast<std::uint16_t>(
            in.start_round_1 - in.now);
        r.wins      = in.wins;
        r.losses    = in.losses;
        r.ties      = in.ties;
        r.unknown3  = 0x08;
        r.selection = 2;
        return r;
    }

    if (!in.in_finals) {
        // Type 5: prelim over, did not make finals.
        r.type      = 5;
        r.wins      = in.wins;
        r.losses    = in.losses;
        r.ties      = in.ties;
        r.unknown3  = 0x04;
        r.selection = 2;
        return r;
    }

    // Types 6 and 7 are dead in the legacy code (`else if ((0))`); we
    // do not implement them either, falling back to type 5 semantics.
    r.type      = 5;
    r.wins      = in.wins;
    r.losses    = in.losses;
    r.ties      = in.ties;
    r.unknown3  = 0x04;
    r.selection = 2;
    return r;
}

}  // namespace pvpgn::application::tournament
