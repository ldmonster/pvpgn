// SPDX-License-Identifier: GPL-2.0-or-later
#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "core/bytes.hpp"

using namespace pvpgn::core;

TEST_CASE("to_hex round-trip", "[core][bytes]") {
    const std::string_view src = "PvPGN";
    const auto hex = to_hex(as_byte_view(src));
    REQUIRE(hex == "507650474e");
    auto back = from_hex(hex);
    REQUIRE(back);
    REQUIRE(back.value().size() == src.size());
}

TEST_CASE("from_hex rejects odd length", "[core][bytes]") {
    auto r = from_hex("abc");
    REQUIRE_FALSE(r);
    REQUIRE(r.error().code() == StatusCode::InvalidArgument);
}

TEST_CASE("from_hex rejects bad chars", "[core][bytes]") {
    auto r = from_hex("zz");
    REQUIRE_FALSE(r);
    REQUIRE(r.error().code() == StatusCode::InvalidArgument);
}

TEST_CASE("from_hex accepts mixed case", "[core][bytes]") {
    auto r = from_hex("aBcDeF");
    REQUIRE(r);
    REQUIRE(r.value().size() == 3);
    REQUIRE(static_cast<unsigned>(r.value()[0]) == 0xAB);
    REQUIRE(static_cast<unsigned>(r.value()[1]) == 0xCD);
    REQUIRE(static_cast<unsigned>(r.value()[2]) == 0xEF);
}
