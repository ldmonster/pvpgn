// SPDX-License-Identifier: GPL-2.0-or-later
//
// Round-trip tests for the per-tag typed payload parsers of
// FINDANONGAME INFOREPLY (URL / MAP / TYPE / DESC / LADR). These work on
// the *decompressed* payload bytes; the on-wire zlib layer is the
// caller's responsibility.

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "protocol/bnet/anongame_tags.hpp"

using namespace pvpgn;
using namespace pvpgn::protocol::bnet;

// ============================================================ URL ========

TEST_CASE("anongame_tags: URL 3 strings (<1.15)",
          "[protocol][bnet][anongame][tags]") {
    AnonGameUrlPayload in{};
    in.urls = {"http://server", "http://player", "http://tourney"};
    auto bytes = serialize_url_payload(in);
    auto out = parse_url_payload(bytes, /*expected_count=*/3);
    REQUIRE(out.has_value());
    REQUIRE(out.value() == in);
}

TEST_CASE("anongame_tags: URL 4 strings (>=1.15)",
          "[protocol][bnet][anongame][tags]") {
    AnonGameUrlPayload in{};
    in.urls = {"http://server", "http://player", "http://tourney",
               "http://clan"};
    auto bytes = serialize_url_payload(in);
    auto out = parse_url_payload(bytes, /*expected_count=*/4);
    REQUIRE(out.has_value());
    REQUIRE(out.value() == in);
}

TEST_CASE("anongame_tags: URL count mismatch fails",
          "[protocol][bnet][anongame][tags]") {
    AnonGameUrlPayload in{};
    in.urls = {"a", "b"};
    auto bytes = serialize_url_payload(in);
    auto out = parse_url_payload(bytes, /*expected_count=*/3);
    REQUIRE_FALSE(out.has_value());
    REQUIRE(out.error().code() == core::StatusCode::OutOfRange);
}

// ============================================================ MAP ========

TEST_CASE("anongame_tags: MAP empty list", "[protocol][bnet][anongame][tags]") {
    AnonGameMapPayload in{};
    auto bytes = serialize_map_payload(in);
    auto out = parse_map_payload(bytes);
    REQUIRE(out.has_value());
    REQUIRE(out.value() == in);
}

TEST_CASE("anongame_tags: MAP multiple names", "[protocol][bnet][anongame][tags]") {
    AnonGameMapPayload in{};
    in.mapnames = {
        "Maps\\FrozenThrone\\(4)Floodplains1v1.w3x",
        "Maps\\FrozenThrone\\(6)ScorchedBasin.w3x",
        "Maps\\FrozenThrone\\(8)Battleground.w3x",
    };
    auto bytes = serialize_map_payload(in);
    auto out = parse_map_payload(bytes);
    REQUIRE(out.has_value());
    REQUIRE(out.value() == in);
}

TEST_CASE("anongame_tags: MAP short buffer fails",
          "[protocol][bnet][anongame][tags]") {
    auto out = parse_map_payload({});  // missing count byte
    REQUIRE_FALSE(out.has_value());
}

// ============================================================ TYPE =======

TEST_CASE("anongame_tags: TYPE single PG section",
          "[protocol][bnet][anongame][tags]") {
    AnonGameTypePayload in{};
    AnonGameTypeSection sec{};
    sec.section_id = 0;  // PG
    AnonGameTypeGamestyle gs1{};
    gs1.prefix      = {0x00, 0x00, 0x03, 0x3F, 0x00};  // PG 1v1
    gs1.map_indices = {2, 0, 1, 3};
    AnonGameTypeGamestyle gs2{};
    gs2.prefix      = {0x01, 0x00, 0x02, 0x3F, 0x00};  // PG 2v2
    gs2.map_indices = {2, 5, 7};
    sec.gamestyles  = {gs1, gs2};
    in.sections     = {sec};

    auto bytes = serialize_type_payload(in);
    auto out = parse_type_payload(bytes);
    REQUIRE(out.has_value());
    REQUIRE(out.value() == in);
}

TEST_CASE("anongame_tags: TYPE multi-section PG+AT+TY",
          "[protocol][bnet][anongame][tags]") {
    AnonGameTypePayload in{};
    AnonGameTypeSection pg{};
    pg.section_id = 0;
    pg.gamestyles = {{{0x00, 0x00, 0x03, 0x3F, 0x00}, {1, 0}}};
    AnonGameTypeSection at{};
    at.section_id = 2;
    at.gamestyles = {{{0x00, 0x00, 0x02, 0x3F, 0x02}, {3, 4, 5}}};
    AnonGameTypeSection ty{};
    ty.section_id = 1;
    ty.gamestyles = {{{0x00, 0x01, 0x00, 0x3F, 0x00}, {0}}};
    in.sections   = {pg, at, ty};

    auto bytes = serialize_type_payload(in);
    auto out = parse_type_payload(bytes);
    REQUIRE(out.has_value());
    REQUIRE(out.value() == in);
}

TEST_CASE("anongame_tags: TYPE empty", "[protocol][bnet][anongame][tags]") {
    AnonGameTypePayload in{};
    auto bytes = serialize_type_payload(in);
    REQUIRE(bytes.size() == 1);  // just the section count
    REQUIRE(bytes[0] == 0);
    auto out = parse_type_payload(bytes);
    REQUIRE(out.has_value());
    REQUIRE(out.value() == in);
}

// ============================================================ DESC =======

TEST_CASE("anongame_tags: DESC entries", "[protocol][bnet][anongame][tags]") {
    AnonGameDescPayload in{};
    in.entries = {
        {0, 0, "1v1",   "One vs. One"},
        {0, 1, "2v2",   "Two vs. Two"},
        {2, 0, "AT 2v2","Arranged Team 2v2"},
        {1, 0, "TY",    "Tournament Game"},
    };
    auto bytes = serialize_desc_payload(in);
    auto out = parse_desc_payload(bytes);
    REQUIRE(out.has_value());
    REQUIRE(out.value() == in);
}

TEST_CASE("anongame_tags: DESC empty entries", "[protocol][bnet][anongame][tags]") {
    AnonGameDescPayload in{};
    auto bytes = serialize_desc_payload(in);
    REQUIRE(bytes.size() == 1);
    auto out = parse_desc_payload(bytes);
    REQUIRE(out.has_value());
    REQUIRE(out.value() == in);
}

// ============================================================ LADR =======

TEST_CASE("anongame_tags: LADR 10 entries (legacy layout)",
          "[protocol][bnet][anongame][tags]") {
    AnonGameLadrPayload in{};
    in.entries = {
        {0x534F4C4Fu /*'OLOS'*/, "PG 1v1",    "http://l/pg1v1"},
        {0x5445414Du /*'MAET'*/, "PG team",   "http://l/pgteam"},
        {0x20464641u /*' AFF'*/, "PG FFA",    "http://l/pgffa"},
        {0x32565332u /*'2SV2'*/, "AT 2v2",    "http://l/at2v2"},
        {0x33565333u /*'3SV3'*/, "AT 3v3",    "http://l/at3v3"},
        {0x34565334u /*'4SV4'*/, "AT 4v4",    "http://l/at4v4"},
        {0x434C4E53u /*'SNLC'*/, "Clan 1v1",  "http://l/cln1v1"},
        {0x434C4E32u /*'2NLC'*/, "Clan 2v2",  "http://l/cln2v2"},
        {0x434C4E33u /*'3NLC'*/, "Clan 3v3",  "http://l/cln3v3"},
        {0x434C4E34u /*'4NLC'*/, "Clan 4v4",  "http://l/cln4v4"},
    };
    auto bytes = serialize_ladr_payload(in);
    auto out = parse_ladr_payload(bytes);
    REQUIRE(out.has_value());
    REQUIRE(out.value() == in);
}

TEST_CASE("anongame_tags: LADR empty", "[protocol][bnet][anongame][tags]") {
    AnonGameLadrPayload in{};
    auto bytes = serialize_ladr_payload(in);
    REQUIRE(bytes.size() == 1);
    auto out = parse_ladr_payload(bytes);
    REQUIRE(out.has_value());
    REQUIRE(out.value() == in);
}

TEST_CASE("anongame_tags: LADR truncated buffer fails",
          "[protocol][bnet][anongame][tags]") {
    // count=2 but only 1 byte after — short read of the first tag.
    std::vector<std::uint8_t> bytes = {0x02, 0x4F};
    auto out = parse_ladr_payload(bytes);
    REQUIRE_FALSE(out.has_value());
}

// ============================================================ trailing ==

TEST_CASE("anongame_tags: trailing bytes after MAP entries -> OutOfRange",
          "[protocol][bnet][anongame][tags]") {
    // count=1, one name, then an unexpected extra byte.
    std::vector<std::uint8_t> bytes = {0x01, 'a', '\0', 0xFF};
    auto out = parse_map_payload(bytes);
    REQUIRE_FALSE(out.has_value());
    REQUIRE(out.error().code() == core::StatusCode::OutOfRange);
}
