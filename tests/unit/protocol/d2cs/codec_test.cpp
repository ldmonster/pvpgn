// SPDX-License-Identifier: GPL-2.0-or-later
#include <variant>

#include <catch2/catch_test_macros.hpp>

#include "protocol/d2cs/codec.hpp"

using namespace pvpgn;
using namespace pvpgn::protocol::d2cs;

TEST_CASE("d2cs: LoginReq round-trip", "[protocol][d2cs]") {
    LoginReq in{};
    in.seqno       = 0x100u;
    in.bncs_addr1  = 0x0100007fu;
    in.session_num = 7u;
    in.session_key = 0u;
    in.cdkey_id    = 0xABCDEF01u;
    in.client_tag  = 0x44324456u;  // 'D2DV'
    in.bn_version  = 0xCAFEu;
    in.bncs_addr2  = 0x0100007fu;
    in.secret_hash = {1, 2, 3, 4, 5};
    in.account_name = "alice";

    protocol::Writer w;
    REQUIRE(encode(w, in).has_value());
    auto m = decode_client(w.view());
    REQUIRE(m.has_value());
    REQUIRE(std::get<LoginReq>(m.value()) == in);
}

TEST_CASE("d2cs: LoginReply round-trip", "[protocol][d2cs]") {
    LoginReply in{kLoginReplyBadPass};
    protocol::Writer w;
    REQUIRE(encode(w, in).has_value());
    auto m = decode_server(w.view());
    REQUIRE(m.has_value());
    REQUIRE(std::get<LoginReply>(m.value()) == in);
}

TEST_CASE("d2cs: parse_header rejects size < header",
          "[protocol][d2cs]") {
    std::array<std::byte, 3> raw{
        std::byte{0x01}, std::byte{0x00}, std::byte{0x01}};
    auto r = parse_header(core::ByteView{raw});
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error().code() == core::StatusCode::InvalidArgument);
}

TEST_CASE("d2cs: CreateCharReq round-trip", "[protocol][d2cs]") {
    CreateCharReq in{1u, 0u, 0x100u, "bob"};
    protocol::Writer w;
    REQUIRE(encode(w, in).has_value());
    auto m = decode_client(w.view());
    REQUIRE(m.has_value());
    REQUIRE(std::get<CreateCharReq>(m.value()) == in);
}

TEST_CASE("d2cs: CreateCharReply round-trip", "[protocol][d2cs]") {
    CreateCharReply in{kCreateCharReplyAlreadyExists};
    protocol::Writer w;
    REQUIRE(encode(w, in).has_value());
    auto m = decode_server(w.view());
    REQUIRE(m.has_value());
    REQUIRE(std::get<CreateCharReply>(m.value()) == in);
}

TEST_CASE("d2cs: CreateGameReq round-trip", "[protocol][d2cs]") {
    CreateGameReq in{};
    in.seqno = 7;
    in.gameflag = 0x10u;
    in.u1 = 1;
    in.leveldiff = 5;
    in.maxchar = 8;
    in.game_name = "g1";
    in.game_pass = "";
    in.game_desc = "fun";
    protocol::Writer w;
    REQUIRE(encode(w, in).has_value());
    auto m = decode_client(w.view());
    REQUIRE(m.has_value());
    REQUIRE(std::get<CreateGameReq>(m.value()) == in);
}

TEST_CASE("d2cs: CreateGameReply round-trip", "[protocol][d2cs]") {
    CreateGameReply in{42u, 99u, 0u, kCreateGameReplyOk};
    protocol::Writer w;
    REQUIRE(encode(w, in).has_value());
    auto m = decode_server(w.view());
    REQUIRE(m.has_value());
    REQUIRE(std::get<CreateGameReply>(m.value()) == in);
}

TEST_CASE("d2cs: JoinGameReq round-trip", "[protocol][d2cs]") {
    JoinGameReq in{7u, "myGame", ""};
    protocol::Writer w;
    REQUIRE(encode(w, in).has_value());
    auto m = decode_client(w.view());
    REQUIRE(m.has_value());
    REQUIRE(std::get<JoinGameReq>(m.value()) == in);
}

TEST_CASE("d2cs: JoinGameReply round-trip", "[protocol][d2cs]") {
    JoinGameReply in{1u, 99u, 0u, 0x0100007Fu, 0xDEADBEEFu, kJoinGameReplyFull};
    protocol::Writer w;
    REQUIRE(encode(w, in).has_value());
    auto m = decode_server(w.view());
    REQUIRE(m.has_value());
    REQUIRE(std::get<JoinGameReply>(m.value()) == in);
}
