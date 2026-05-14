// SPDX-License-Identifier: GPL-2.0-or-later
#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <vector>

#include "application/profile/profile_reply.hpp"
#include "protocol/bnet/anongame.hpp"

namespace papp = pvpgn::application::profile;
namespace pb   = pvpgn::protocol::bnet;

TEST_CASE("profile reply: no-stats path emits 2 trailing zero bytes",
          "[application][profile]") {
    papp::ProfileInputs in;
    in.count        = 0xCAFEBABEu;
    in.profile_icon = 0x12345678u;
    in.has_stats    = false;

    auto r = papp::build_profile_reply(in);

    REQUIRE(r.count    == 0xCAFEBABEu);
    REQUIRE(r.icon     == 0x12345678u);
    REQUIRE(r.rescount == 0);
    REQUIRE(r.data.size() == 2);
    REQUIRE(r.data[0] == 0x00);
    REQUIRE(r.data[1] == 0x00);
}

TEST_CASE("profile reply: solo-only sets rescount=1 + race section",
          "[application][profile]") {
    papp::ProfileInputs in;
    in.count        = 1;
    in.profile_icon = 0;
    in.has_stats    = true;

    in.solo.wins   = 0x0102;
    in.solo.losses = 0x0304;
    in.solo.level  = 7;
    in.solo.calc   = 0x55;
    in.solo.xp     = 0x0A0B;
    in.solo.rank   = 0xDEADBEEFu;

    in.humans.wins = 0x4142;

    auto r = papp::build_profile_reply(in);

    REQUIRE(r.rescount == 1);
    REQUIRE(r.data.size() == 16 /*solo*/ + 1 /*race hdr*/ + 24 /*6 races*/ + 1 /*team count*/);

    // Solo section: tag "SOLO" (LE 0x534F4C4F).
    REQUIRE(r.data[0] == 0x4F);
    REQUIRE(r.data[1] == 0x4C);
    REQUIRE(r.data[2] == 0x4F);
    REQUIRE(r.data[3] == 0x53);
    REQUIRE(r.data[4] == 0x02);  // wins lo
    REQUIRE(r.data[5] == 0x01);  // wins hi
    REQUIRE(r.data[6] == 0x04);  // losses lo
    REQUIRE(r.data[7] == 0x03);
    REQUIRE(r.data[8] == 7);     // level
    REQUIRE(r.data[9] == 0x55);  // calc
    REQUIRE(r.data[10] == 0x0B); // xp lo
    REQUIRE(r.data[11] == 0x0A);
    REQUIRE(r.data[12] == 0xEF); // rank LE
    REQUIRE(r.data[13] == 0xBE);
    REQUIRE(r.data[14] == 0xAD);
    REQUIRE(r.data[15] == 0xDE);

    // Race header byte.
    REQUIRE(r.data[16] == 0x06);

    // Random wins/losses (zero), then humans wins LE 0x4142.
    REQUIRE(r.data[17] == 0x00);  // random wins lo
    REQUIRE(r.data[18] == 0x00);
    REQUIRE(r.data[19] == 0x00);  // random losses
    REQUIRE(r.data[20] == 0x00);
    REQUIRE(r.data[21] == 0x42);  // humans wins lo
    REQUIRE(r.data[22] == 0x41);  // humans wins hi

    // Team count byte at the very end.
    REQUIRE(r.data.back() == 0x00);
}

TEST_CASE("profile reply: all three ladders + AT team", "[application][profile]") {
    papp::ProfileInputs in;
    in.count = 0;
    in.has_stats = true;
    in.solo.level = 1;
    in.team.level = 2;
    in.ffa .level = 3;

    papp::ATTeamRecord t{};
    t.team_tag       = 0x32565332u;  // "2VS2" LE -> "2SV2" on wire
    t.wins           = 0x0001;
    t.losses         = 0x0002;
    t.level          = 4;
    t.calc           = 0x33;
    t.xp             = 0x0003;
    t.rank           = 0x00000004u;
    t.lastgame_bn_long = {0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80};
    t.size_minus_one = 1;
    t.other_members  = {"buddy"};
    in.teams.push_back(t);

    auto r = papp::build_profile_reply(in);
    REQUIRE(r.rescount == 3);

    // 3 ladder sections (48) + race hdr (1) + 6 races (24)
    // + team count (1) + per-team [tag(4)+w(2)+l(2)+lvl(1)+calc(1)+xp(2)
    // + rank(4) + lastgame(8) + size-1(1)] (25) + "buddy\0" (6)
    // = 48 + 1 + 24 + 1 + 25 + 6 = 105
    REQUIRE(r.data.size() == 105);
    REQUIRE(r.data[48] == 0x06);  // race header

    // Team count immediately after race section.
    REQUIRE(r.data[48 + 1 + 24] == 0x01);

    // Verify trailing member name "buddy\0".
    REQUIRE(r.data[105 - 6] == 'b');
    REQUIRE(r.data[105 - 5] == 'u');
    REQUIRE(r.data[105 - 4] == 'd');
    REQUIRE(r.data[105 - 3] == 'd');
    REQUIRE(r.data[105 - 2] == 'y');
    REQUIRE(r.data[105 - 1] == 0x00);
}

TEST_CASE("profile reply: caps AT team list at 16", "[application][profile]") {
    papp::ProfileInputs in;
    in.has_stats = true;
    in.solo.level = 1;  // ensure has_stats path
    for (int i = 0; i < 25; ++i) {
        papp::ATTeamRecord t{};
        t.team_tag = 0x32565332u;
        t.size_minus_one = 1;
        in.teams.push_back(t);
    }
    auto r = papp::build_profile_reply(in);
    // Find the team count byte: 1 ladder (16) + race hdr (1) + 6 races (24) = 41
    REQUIRE(r.data[41] == 16);
}

// --- 15d sliver: builder -> serialize -> parse_findanongame_reply round-trip
//
// Acts as a smoke test that the bridge's pipeline (build -> serialise into
// the typed envelope -> codec) round-trips a non-trivial PROFILE reply.
// True bridge-level integration tests would require linking against
// bnetd_legacy + standing up a t_connection / t_account; that scaffolding
// is deferred. This test guards the in-process layers the bridge actually
// composes.
TEST_CASE("profile reply: builder output round-trips through anongame codec",
          "[application][profile][integration]") {
    papp::ProfileInputs in;
    in.count        = 99;
    in.profile_icon = 0xDEADBEEFu;
    in.has_stats    = true;
    in.team.wins   = 0x0010;
    in.team.losses = 0x0020;
    in.team.level  = 9;
    in.team.calc   = 0x77;
    in.team.xp     = 0x0030;
    in.team.rank   = 0x12345678u;
    in.orcs.wins   = 0x55;

    pb::AnonGameProfileReply built = papp::build_profile_reply(in);

    // Wrap into the 0x44 envelope and parse it back via the typed codec.
    auto env = pb::serialize_findanongame_reply(pb::AnonGameServer{built});
    auto parsed = pb::parse_findanongame_reply(env);
    REQUIRE(parsed.has_value());

    auto const* round_tripped = std::get_if<pb::AnonGameProfileReply>(&parsed.value());
    REQUIRE(round_tripped != nullptr);
    REQUIRE(*round_tripped == built);
}
