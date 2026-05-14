// SPDX-License-Identifier: GPL-2.0-or-later
//
// Round-trip tests for the FINDANONGAME (0x44) typed sub-message layer.
// These exercise the second-pass parse/serialize that sits on top of the
// opaque `WarcraftGeneralRequest` / `WarcraftGeneralReply` envelopes.

#include <array>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "protocol/bnet/anongame.hpp"
#include "protocol/bnet/codec.hpp"
#include "protocol/bnet/messages.hpp"
#include "protocol/common/packet.hpp"
#include "protocol/common/writer.hpp"

using namespace pvpgn;
using namespace pvpgn::protocol::bnet;

namespace {

// Two-layer round-trip: typed → envelope → wire → envelope → typed.
template <class Typed>
Typed full_round_trip_client(const Typed& in) {
    auto env = serialize_findanongame_request(AnonGameClient{in});
    protocol::Writer w;
    REQUIRE(encode(w, env).has_value());
    auto fp = protocol::parse_packet(w.view());
    REQUIRE(fp.has_value());
    auto cm = decode_client(fp.value().packet);
    REQUIRE(cm.has_value());
    const auto& env2 = std::get<WarcraftGeneralRequest>(cm.value());
    auto typed = parse_findanongame_request(env2);
    REQUIRE(typed.has_value());
    return std::get<Typed>(typed.value());
}

template <class Typed>
Typed full_round_trip_server(const Typed& in) {
    auto env = serialize_findanongame_reply(AnonGameServer{in});
    protocol::Writer w;
    REQUIRE(encode(w, env).has_value());
    auto fp = protocol::parse_packet(w.view());
    REQUIRE(fp.has_value());
    auto sm = decode_server(fp.value().packet);
    REQUIRE(sm.has_value());
    const auto& env2 = std::get<WarcraftGeneralReply>(sm.value());
    auto typed = parse_findanongame_reply(env2);
    REQUIRE(typed.has_value());
    return std::get<Typed>(typed.value());
}

}  // namespace

TEST_CASE("anongame: 0x00 client SEARCH (PG 1v1)", "[protocol][bnet][anongame]") {
    AnonGameSearch in{};
    in.count     = 1;
    in.unknown2  = 0;
    in.type      = 0;
    in.gametype  = 0;
    in.map_prefs = 0xFF000000u;
    in.unknown3  = 0x08;
    in.id        = 0x02A613C2u;
    in.race      = 0x20;
    REQUIRE(full_round_trip_client(in) == in);
}

TEST_CASE("anongame: 0x05 client AT_SEARCH (3v3)", "[protocol][bnet][anongame]") {
    AnonGameAtSearch in{};
    in.count     = 1;
    in.tid       = 0x19C436B7u;
    in.timestamp = 0x032F021Bu;
    in.teamsize  = 3;
    in.info      = {0x19, 2, 1, 0xFFFFFFFFu, 0xFFFFFFFFu};
    in.unknown2  = 0;
    in.unknown3  = 0x08;
    in.id        = 0x0EC659AAu;
    in.race      = 0x20;
    REQUIRE(full_round_trip_client(in) == in);
}

TEST_CASE("anongame: 0x06 client AT_INVITER_SEARCH (2v2)", "[protocol][bnet][anongame]") {
    AnonGameAtInviterSearch in{};
    in.count     = 9;
    in.tid       = 0x3A0u;
    in.timestamp = 0x2740720Cu;
    in.teamsize  = 2;
    in.info      = {0x6BDAB902u, 0x2249A4A4u, 0xEF496064u, 0x256F4415u, 0x13F94E02u};
    in.unknown2  = 0x4DBCu;
    in.type      = 1;
    in.gametype  = 0;
    in.map_prefs = 0x000007FFu;
    in.unknown3  = 0x08;
    in.id        = 0x04E6987Eu;
    in.race      = 4;
    REQUIRE(full_round_trip_client(in) == in);
}

TEST_CASE("anongame: 0x02 client INFOREQ", "[protocol][bnet][anongame]") {
    AnonGameInfoRequest in{};
    in.count   = 1;
    in.noitems = 4;
    in.entries = {
        {kAnonGameInfoTagURL,  0x11111111u},
        {kAnonGameInfoTagMAP,  0x22222222u},
        {kAnonGameInfoTagTYPE, 0x33333333u},
        {kAnonGameInfoTagDESC, 0x44444444u},
    };
    REQUIRE(full_round_trip_client(in) == in);
}

TEST_CASE("anongame: 0x02 client INFOREQ empty entries", "[protocol][bnet][anongame]") {
    AnonGameInfoRequest in{1u, 0, {}};
    REQUIRE(full_round_trip_client(in) == in);
}

TEST_CASE("anongame: 0x03 client cancel", "[protocol][bnet][anongame]") {
    AnonGameClientCancel in{42u};
    REQUIRE(full_round_trip_client(in) == in);
}

TEST_CASE("anongame: 0x04 client profile request", "[protocol][bnet][anongame]") {
    AnonGameProfileRequest in{};
    in.count      = 7;
    in.username   = "Alice";
    in.client_tag = "W3XP";
    REQUIRE(full_round_trip_client(in) == in);
}

TEST_CASE("anongame: 0x07 client tournament request", "[protocol][bnet][anongame]") {
    AnonGameTournamentRequest in{1u};
    REQUIRE(full_round_trip_client(in) == in);
}

TEST_CASE("anongame: 0x08 client clan profile", "[protocol][bnet][anongame]") {
    AnonGameClanProfileRequest in{3u, 0x434C4E31u /* "CLN1" */};
    REQUIRE(full_round_trip_client(in) == in);
}

TEST_CASE("anongame: 0x09 client GET_ICON", "[protocol][bnet][anongame]") {
    AnonGameGetIcon in{5u};
    REQUIRE(full_round_trip_client(in) == in);
}

TEST_CASE("anongame: 0x0A client SET_ICON", "[protocol][bnet][anongame]") {
    AnonGameSetIcon in{5u, 0x4F485200u /* "OHR\0" */};
    REQUIRE(full_round_trip_client(in) == in);
}

TEST_CASE("anongame: 0x00 server SEARCH reply", "[protocol][bnet][anongame]") {
    AnonGameSearchReply in{2u, 0u};
    REQUIRE(full_round_trip_server(in) == in);
}

TEST_CASE("anongame: 0x01 server FOUND (PG 2v2)", "[protocol][bnet][anongame]") {
    AnonGameFound in{};
    in.count     = 2;
    in.unknown1  = 0;
    in.ip_be     = 0x0017E0DCu;
    in.port_be   = 0x672Eu;
    in.unknown2  = 0;
    in.unknown3  = 0;
    in.unknown4  = 0x0F56u;
    in.id        = 0x06007D9Au;
    in.unknown5  = 0x06;
    in.type      = 0;
    in.gametype  = 1;
    in.mapname   = "Maps\\FrozenThrone\\(6)ScorchedBasin.w3x";
    in.saf = {0xFFFFFFFFu, 0x5445414Du /* 'MAET'='TEAM' */,
              /*totalplayers*/4, /*totalteams*/2, /*unknown2*/0,
              /*visibility*/2, /*unknown3*/2};
    REQUIRE(full_round_trip_server(in) == in);
}

TEST_CASE("anongame: 0x01 server FOUND preserves extras tail",
          "[protocol][bnet][anongame]") {
    AnonGameFound in{};
    in.count    = 1;
    in.ip_be    = 0x01020304u;
    in.port_be  = 0x1F90u;
    in.id       = 42;
    in.unknown5 = 0x06;
    in.mapname  = "Maps\\Test.w3x";
    in.saf      = {0xFFFFFFFFu, 0x4F4C4F53u /* 'SOLO' */, 1, 0, 0, 2, 2};
    in.extras   = {0xAA, 0xBB, 0xCC};
    REQUIRE(full_round_trip_server(in) == in);
}

TEST_CASE("anongame: 0x03 server cancel", "[protocol][bnet][anongame]") {
    AnonGameServerCancel in{7u};
    REQUIRE(full_round_trip_server(in) == in);
}

TEST_CASE("anongame: 0x02 server INFOREPLY URL tag",
          "[protocol][bnet][anongame]") {
    AnonGameInfoReply in{};
    in.count    = 1;
    in.noitems  = 1;
    in.tag      = kAnonGameInfoTagServerURL;
    in.tag_unk  = 0xBF1F1047u;
    // legacy URL payload is 3 NUL-terminated strings
    in.payload  = {'h','t','t','p','s',':','/','/','a','\0',
                   'b','\0','c','\0'};
    in.trailing = 0x00;  // last packet in group
    REQUIRE(full_round_trip_server(in) == in);
}

TEST_CASE("anongame: 0x02 server INFOREPLY non-last (trailing=1)",
          "[protocol][bnet][anongame]") {
    AnonGameInfoReply in{};
    in.count    = 1;
    in.noitems  = 1;
    in.tag      = kAnonGameInfoTagServerMAP;
    in.tag_unk  = 0x70E2E0D5u;
    in.payload  = {'(','4',')','M','a','p','.','w','3','x','\0'};
    in.trailing = 0x01;
    REQUIRE(full_round_trip_server(in) == in);
}

TEST_CASE("anongame: 0x02 server INFOREPLY empty payload",
          "[protocol][bnet][anongame]") {
    AnonGameInfoReply in{};
    in.count    = 1;
    in.noitems  = 0;
    in.tag      = kAnonGameInfoTagServerDESC;
    in.tag_unk  = 0xA4F0A22Fu;
    in.payload  = {};
    in.trailing = 0x00;
    REQUIRE(full_round_trip_server(in) == in);
}

TEST_CASE("anongame: 0x04 server PROFILE2 (opaque tail)",
          "[protocol][bnet][anongame]") {
    AnonGameProfileReply in{};
    in.count    = 4;
    in.icon     = 0x57415233u;
    in.rescount = 2;
    in.data     = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    REQUIRE(full_round_trip_server(in) == in);
}

TEST_CASE("anongame: 0x07 server tournament reply", "[protocol][bnet][anongame]") {
    AnonGameTournamentReply in{};
    in.count     = 1;
    in.type      = 2;
    in.unknown1  = 0;
    in.unknown4  = 0;
    in.timestamp = 0x65000000u;
    in.unknown5  = 1;
    in.countdown = 600;
    in.unknown2  = 0;
    in.wins      = 3;
    in.losses    = 1;
    in.ties      = 0;
    in.unknown3  = 8;
    in.selection = 1;
    in.descnum   = 1;
    in.nulltag   = 0;
    REQUIRE(full_round_trip_server(in) == in);
}

TEST_CASE("anongame: 0x09 server icon reply (WAR3 5x4)",
          "[protocol][bnet][anongame]") {
    AnonGameIconReply in{};
    in.count       = 7;
    in.curricon    = {'1', 'H', '3', 'W'};
    in.table_width = 5;
    in.table_size  = 20;
    for (std::uint8_t r = 0; r < 4; ++r) {
        for (std::uint8_t c = 0; c < 5; ++c) {
            AnonGameIconReplyEntry e{};
            e.icon_code      = {static_cast<char>('2' + r),
                                "RHOUN"[c], '3', 'W'};
            e.portrait_code  = 0xDEADBEEFu + r * 16u + c;
            e.race           = c;
            e.required_wins  = static_cast<std::uint16_t>(25 * (r + 1));
            e.client_enabled = (r == 0) ? 1u : 0u;
            in.entries.push_back(e);
        }
    }
    REQUIRE(full_round_trip_server(in) == in);
}

TEST_CASE("anongame: 0x09 server icon reply (W3XP 6x5 with tourney column)",
          "[protocol][bnet][anongame]") {
    AnonGameIconReply in{};
    in.count       = 11;
    in.curricon    = {'2', 'O', '3', 'W'};
    in.table_width = 6;
    in.table_size  = 30;
    for (std::uint8_t r = 0; r < 5; ++r) {
        for (std::uint8_t c = 0; c < 6; ++c) {
            AnonGameIconReplyEntry e{};
            e.icon_code      = {static_cast<char>('2' + r),
                                "RHOUND"[c], '3', 'W'};
            e.portrait_code  = 0x12345678u ^ (r * 256u + c);
            e.race           = c;
            // Tourney column uses a different threshold series.
            e.required_wins  = (c == 5)
                ? static_cast<std::uint16_t>(10 * (r + 1) * (r + 1))
                : static_cast<std::uint16_t>(50 * (r + 1));
            e.client_enabled = ((c + r) % 2 == 0) ? 1u : 0u;
            in.entries.push_back(e);
        }
    }
    REQUIRE(full_round_trip_server(in) == in);
}

TEST_CASE("anongame: 0x08 server clan profile reply (stub trailer)",
          "[protocol][bnet][anongame]") {
    AnonGameClanProfileReply in{};
    in.count    = 0x12345678u;
    in.rescount = 0;
    in.trailer  = {0x00};
    REQUIRE(full_round_trip_server(in) == in);
}

TEST_CASE("anongame: unimplemented client sub-option returns Unimplemented",
          "[protocol][bnet][anongame]") {
    WarcraftGeneralRequest env{};
    env.sub_option = 0x7F;
    auto v = parse_findanongame_request(env);
    REQUIRE_FALSE(v.has_value());
    REQUIRE(v.error().code() == core::StatusCode::Unimplemented);
}

TEST_CASE("anongame: unimplemented server sub-option returns Unimplemented",
          "[protocol][bnet][anongame]") {
    WarcraftGeneralReply env{};
    env.sub_option = 0x7F;
    auto v = parse_findanongame_reply(env);
    REQUIRE_FALSE(v.has_value());
    REQUIRE(v.error().code() == core::StatusCode::Unimplemented);
}

TEST_CASE("anongame: 0x09 server icon reply golden bytes (1 entry)",
          "[protocol][bnet][anongame][golden]") {
    // Single-entry IconReply lets us hand-compute the expected
    // wire bytes and lock the layout (LE / BE / field order).
    AnonGameIconReply in{};
    in.count       = 0x01020304u;
    in.curricon    = {'1', 'O', '3', 'W'};
    in.table_width = 5;
    in.table_size  = 1;
    AnonGameIconReplyEntry e{};
    e.icon_code      = {'2', 'H', '3', 'W'};
    e.portrait_code  = 0xAABBCCDDu;
    e.race           = 0x05;
    e.required_wins  = 0x1234;  // BE on wire -> 0x12 0x34
    e.client_enabled = 0x01;
    in.entries.push_back(e);

    auto env = serialize_findanongame_reply(AnonGameServer{in});
    protocol::Writer w;
    REQUIRE(encode(w, env).has_value());
    auto view = w.view();

    // Expected layout:
    //   FF 44 sz_lo sz_hi              BNet header
    //   09                             sub_option
    //   04 03 02 01                    count (LE)
    //   31 4F 33 57                    curricon "1O3W"
    //   05                             table_width
    //   01                             table_size
    //   32 48 33 57                    icon_code "2H3W"
    //   DD CC BB AA                    portrait_code (LE)
    //   05                             race
    //   12 34                          required_wins (BE)
    //   01                             client_enabled
    // total payload after header: 1+4+4+1+1 + 4+4+1+2+1 = 23 bytes
    // total packet size = 4 + 23 = 27 = 0x1B
    const std::uint8_t expected[] = {
        0xFF, 0x44, 0x1B, 0x00,
        0x09,
        0x04, 0x03, 0x02, 0x01,
        0x31, 0x4F, 0x33, 0x57,
        0x05,
        0x01,
        0x32, 0x48, 0x33, 0x57,
        0xDD, 0xCC, 0xBB, 0xAA,
        0x05,
        0x12, 0x34,
        0x01,
    };
    REQUIRE(view.size() == sizeof(expected));
    for (std::size_t i = 0; i < sizeof(expected); ++i) {
        INFO("byte index " << i);
        REQUIRE(static_cast<std::uint8_t>(view[i]) == expected[i]);
    }
}

TEST_CASE("anongame: 0x07 server tournament reply golden bytes (type 2)",
          "[protocol][bnet][anongame][golden]") {
    // Type-2 (signup window) covers most fields. All multi-byte
    // ints LE per `enc_tournament_reply`.
    AnonGameTournamentReply in{};
    in.count    = 7;
    in.type     = 2;
    in.unknown1 = 0;
    in.unknown4 = 0x0828;
    in.timestamp = 0xDEADBEEFu;
    in.unknown5  = 0x01;
    in.countdown = 0x0100;
    in.unknown2  = 0x0000;
    in.wins   = 3;
    in.losses = 1;
    in.ties   = 0;
    in.unknown3 = 0x08;
    in.selection = 2;
    in.descnum   = 0;
    in.nulltag   = 0;

    auto env = serialize_findanongame_reply(AnonGameServer{in});
    protocol::Writer w;
    REQUIRE(encode(w, env).has_value());
    auto view = w.view();

    // Payload after BNet header (4 bytes):
    //   1 sub_option + 4 count + 1 type + 1 unknown1 + 2 unknown4
    //   + 4 timestamp + 1 unknown5 + 2 countdown + 2 unknown2
    //   + 1 wins + 1 losses + 1 ties + 1 unknown3 + 1 selection
    //   + 1 descnum + 1 nulltag = 25
    // Total = 4 + 25 = 29 = 0x1D
    const std::uint8_t expected[] = {
        0xFF, 0x44, 0x1D, 0x00,
        0x07,                            // sub_option
        0x07, 0x00, 0x00, 0x00,          // count
        0x02,                            // type
        0x00,                            // unknown1
        0x28, 0x08,                      // unknown4 LE
        0xEF, 0xBE, 0xAD, 0xDE,          // timestamp LE
        0x01,                            // unknown5
        0x00, 0x01,                      // countdown LE
        0x00, 0x00,                      // unknown2 LE
        0x03,                            // wins
        0x01,                            // losses
        0x00,                            // ties
        0x08,                            // unknown3
        0x02,                            // selection
        0x00,                            // descnum
        0x00,                            // nulltag
    };
    REQUIRE(view.size() == sizeof(expected));
    for (std::size_t i = 0; i < sizeof(expected); ++i) {
        INFO("byte index " << i);
        REQUIRE(static_cast<std::uint8_t>(view[i]) == expected[i]);
    }
}
