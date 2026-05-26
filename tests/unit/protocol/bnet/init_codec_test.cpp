// SPDX-License-Identifier: GPL-2.0-or-later
#include <catch2/catch_test_macros.hpp>

#include "protocol/bnet/init_codec.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace init = pvpgn::protocol::bnet::init;

TEST_CASE("init_codec: kClientInitConnSize is 1", "[protocol][bnet][init][codec]")
{
    REQUIRE(init::kClientInitConnSize == 1u);
}

TEST_CASE("init_codec: parse a single cclass byte (std::byte)",
          "[protocol][bnet][init][codec]")
{
    std::array<std::byte, 1> buf{ static_cast<std::byte>(init::kClassBnet) };
    auto const r = init::parse_client_initconn(buf);
    REQUIRE(r.has_value());
    CHECK(r->cclass == init::kClassBnet);
}

TEST_CASE("init_codec: parse a single cclass byte (std::uint8_t)",
          "[protocol][bnet][init][codec]")
{
    std::array<std::uint8_t, 1> buf{ init::kClassTelnet };
    auto const r = init::parse_client_initconn(std::span<const std::uint8_t>(buf));
    REQUIRE(r.has_value());
    CHECK(r->cclass == init::kClassTelnet);
}

TEST_CASE("init_codec: parse rejects empty buffer",
          "[protocol][bnet][init][codec]")
{
    std::array<std::byte, 0> empty{};
    CHECK_FALSE(init::parse_client_initconn(std::span<const std::byte>(empty)).has_value());
}

TEST_CASE("init_codec: parse rejects buffers larger than one byte",
          "[protocol][bnet][init][codec]")
{
    std::array<std::byte, 2> two{ std::byte{0x01}, std::byte{0x02} };
    CHECK_FALSE(init::parse_client_initconn(two).has_value());

    std::array<std::byte, 8> eight{};
    CHECK_FALSE(init::parse_client_initconn(eight).has_value());
}

TEST_CASE("init_codec: parser does NOT enforce policy (accepts every byte)",
          "[protocol][bnet][init][codec]")
{
    // Every value 0x00..0xFF must parse. Policy (rate-limit /
    // realm-list gate / class dispatch) lives in
    // application/init/init_conn_dispatch -- a parser only
    // enforces framing.
    for (unsigned v = 0; v < 256u; ++v) {
        std::array<std::byte, 1> buf{ static_cast<std::byte>(v) };
        auto const r = init::parse_client_initconn(buf);
        REQUIRE(r.has_value());
        CHECK(r->cclass == static_cast<std::uint8_t>(v));
    }
}

TEST_CASE("init_codec: encode produces the cclass byte",
          "[protocol][bnet][init][codec]")
{
    auto const w = init::encode_client_initconn(init::ClientInitConn{ init::kClassBot });
    REQUIRE(w.size() == 1u);
    CHECK(static_cast<std::uint8_t>(w[0]) == init::kClassBot);
}

TEST_CASE("init_codec: encode/parse roundtrip covers every cclass value",
          "[protocol][bnet][init][codec]")
{
    for (unsigned v = 0; v < 256u; ++v) {
        init::ClientInitConn const p{ static_cast<std::uint8_t>(v) };
        auto const wire = init::encode_client_initconn(p);
        auto const r    = init::parse_client_initconn(wire);
        REQUIRE(r.has_value());
        CHECK(*r == p);
    }
}

TEST_CASE("init_codec: every documented class constant roundtrips",
          "[protocol][bnet][init][codec]")
{
    auto const check = [](std::uint8_t v) {
        init::ClientInitConn const p{ v };
        auto const wire = init::encode_client_initconn(p);
        auto const r    = init::parse_client_initconn(wire);
        REQUIRE(r.has_value());
        CHECK(r->cclass == v);
    };
    check(init::kClassBnet);
    check(init::kClassFile);
    check(init::kClassBot);
    check(init::kClassEnc);
    check(init::kClassTelnet);
    check(init::kClassD2cs);
    check(init::kClassD2gs);
    check(init::kClassD2csBnetd);
    check(init::kClassLocalMachine);
}
