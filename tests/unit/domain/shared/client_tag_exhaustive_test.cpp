// SPDX-License-Identifier: GPL-2.0-or-later

/// @file client_tag_exhaustive_test.cpp
/// Net-new branch coverage for domain::ClientTag beyond client_tag_test.cpp.
/// Targets: error StatusCode on each parse rejection branch, from_packed_be
/// byte-order layout, parse/packed round-trip for the boundary printable
/// chars, title()/is_* for the lang-tag and unknown branches not already
/// hit, and ordering relations between several constants.

#include <catch2/catch_test_macros.hpp>

#include <string>

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/client_tag.hpp"

using pvpgn::core::StatusCode;
using namespace pvpgn::domain;
using namespace pvpgn::domain::tags;

// ---------------------------------------------------------------------------
// parse() error-code branches
// ---------------------------------------------------------------------------

TEST_CASE("ClientTag: parse() wrong length yields InvalidArgument code",
          "[domain][shared][client_tag]") {
    auto r = ClientTag::parse("ABC");
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error().code() == StatusCode::InvalidArgument);
}

TEST_CASE("ClientTag: parse() non-printable yields InvalidArgument code",
          "[domain][shared][client_tag]") {
    auto r = ClientTag::parse(std::string_view{"AB\x1F""D", 4});
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error().code() == StatusCode::InvalidArgument);
}

TEST_CASE("ClientTag: parse() rejects byte just above printable range (0x7F)",
          "[domain][shared][client_tag]") {
    REQUIRE_FALSE(ClientTag::parse(std::string_view{"\x7F""BCD", 4}).has_value());
}

TEST_CASE("ClientTag: parse() rejects high-bit byte (0x80)",
          "[domain][shared][client_tag]") {
    REQUIRE_FALSE(ClientTag::parse(std::string_view{"AB\x80""D", 4}).has_value());
}

TEST_CASE("ClientTag: parse() accepts the exact printable boundaries 0x20 and 0x7E in every slot",
          "[domain][shared][client_tag]") {
    REQUIRE(ClientTag::parse(std::string_view{"    ", 4}).has_value());  // 4x space
    REQUIRE(ClientTag::parse(std::string_view{"~~~~", 4}).has_value());  // 4x tilde
}

// ---------------------------------------------------------------------------
// from_packed_be() byte-order layout
// ---------------------------------------------------------------------------

TEST_CASE("ClientTag: from_packed_be lays bytes most-significant-first",
          "[domain][shared][client_tag]") {
    // 0x41424344 -> 'A','B','C','D'
    auto t = ClientTag::from_packed_be(0x41424344u);
    REQUIRE(t.has_value());
    REQUIRE(t.value().bytes()[0] == 'A');
    REQUIRE(t.value().bytes()[1] == 'B');
    REQUIRE(t.value().bytes()[2] == 'C');
    REQUIRE(t.value().bytes()[3] == 'D');
    REQUIRE(std::string{t.value().text()} == "ABCD");
}

TEST_CASE("ClientTag: from_packed_be rejects packed value with a non-printable low byte",
          "[domain][shared][client_tag]") {
    // last byte 0x1F is non-printable
    auto r = ClientTag::from_packed_be(0x4142431Fu);
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error().code() == StatusCode::InvalidArgument);
}

TEST_CASE("ClientTag: packed_be of every-slot-distinct tag matches manual computation",
          "[domain][shared][client_tag]") {
    auto t = ClientTag::parse("WxYz").value();
    const std::uint32_t expected =
        (static_cast<std::uint32_t>('W') << 24) |
        (static_cast<std::uint32_t>('x') << 16) |
        (static_cast<std::uint32_t>('Y') <<  8) |
         static_cast<std::uint32_t>('z');
    REQUIRE(t.packed_be() == expected);
}

// ---------------------------------------------------------------------------
// Language-tag constants are NOT clients/arch/wol and report Unknown title
// ---------------------------------------------------------------------------

TEST_CASE("ClientTag: language tags are not clients, arch, or WOL",
          "[domain][shared][client_tag]") {
    REQUIRE_FALSE(kLangDeDE.is_valid_client());
    REQUIRE_FALSE(kLangDeDE.is_valid_arch());
    REQUIRE_FALSE(kLangDeDE.is_wol_v1());
    REQUIRE_FALSE(kLangDeDE.is_wol_v2());
    REQUIRE(std::string{kLangDeDE.title()} == "Unknown");
}

TEST_CASE("ClientTag: a freshly parsed unknown tag reports Unknown title and not a client",
          "[domain][shared][client_tag]") {
    auto t = ClientTag::parse("ZZZZ").value();
    REQUIRE(std::string{t.title()} == "Unknown");
    REQUIRE_FALSE(t.is_valid_client());
    REQUIRE_FALSE(t.is_valid_arch());
}

TEST_CASE("ClientTag: kUnknown sentinel is not a WOL client of either version",
          "[domain][shared][client_tag]") {
    REQUIRE_FALSE(kUnknown.is_wol_v1());
    REQUIRE_FALSE(kUnknown.is_wol_v2());
}

// ---------------------------------------------------------------------------
// Cross-checking each arch tag's negative properties
// ---------------------------------------------------------------------------

TEST_CASE("ClientTag: arch tags are not clients and have no title",
          "[domain][shared][client_tag]") {
    REQUIRE_FALSE(kArchOsxPpc.is_valid_client());
    REQUIRE(std::string{kArchOsxPpc.title()} == "Unknown");
    REQUIRE(kArchOsxPpc.is_valid_arch());
}

// ---------------------------------------------------------------------------
// Ordering via spaceship operator beyond the existing single case
// ---------------------------------------------------------------------------

TEST_CASE("ClientTag: ordering is consistent with packed_be magnitude",
          "[domain][shared][client_tag]") {
    // CHAT 0x43484154 < D2DV 0x44324456 < STAR 0x53544152 < WAR3 0x57415233
    REQUIRE(kBnChatBot < kDiablo2);
    REQUIRE(kDiablo2   < kStarcraft);
    REQUIRE(kStarcraft < kWarcraft3);
    REQUIRE_FALSE(kWarcraft3 < kBnChatBot);
}

TEST_CASE("ClientTag: default tag (spaces) orders below any printable-letter tag",
          "[domain][shared][client_tag]") {
    ClientTag def;  // four spaces, 0x20202020
    REQUIRE(def < kBnChatBot);
}

// ---------------------------------------------------------------------------
// from_packed_be / parse equivalence for several known constants
// ---------------------------------------------------------------------------

TEST_CASE("ClientTag: from_packed_be reconstructs Diablo II tag",
          "[domain][shared][client_tag]") {
    auto t = ClientTag::from_packed_be(0x44324456u);  // D2DV
    REQUIRE(t.has_value());
    REQUIRE(t.value() == kDiablo2);
}

TEST_CASE("ClientTag: parse and from_packed_be agree for the same tag",
          "[domain][shared][client_tag]") {
    auto a = ClientTag::parse("SEXP").value();
    auto b = ClientTag::from_packed_be(0x53455850u).value();
    REQUIRE(a == b);
    REQUIRE(a == kBroodWar);
}
