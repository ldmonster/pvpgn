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

TEST_CASE("d2cs: GameListReq round-trip", "[protocol][d2cs]") {
    GameListReq in{0x0042u, 0x00000200u};  // hardcore bit
    protocol::Writer w;
    REQUIRE(encode(w, in).has_value());
    auto m = decode_client(w.view());
    REQUIRE(m.has_value());
    REQUIRE(std::get<GameListReq>(m.value()) == in);
}

TEST_CASE("d2cs: GameListReply round-trip (one game)", "[protocol][d2cs]") {
    GameListReply in;
    in.seqno     = 0x0042u;
    in.token     = 0xDEADBEEFu;
    in.currchar  = 3u;
    in.gameflag  = 0x00000004u;
    in.game_name = "speedrun-1";
    in.game_desc = "Diff: Normal | Lvl: 25";
    protocol::Writer w;
    REQUIRE(encode(w, in).has_value());
    auto m = decode_server(w.view());
    REQUIRE(m.has_value());
    REQUIRE(std::get<GameListReply>(m.value()) == in);
}

TEST_CASE("d2cs: GameInfoReq round-trip", "[protocol][d2cs]") {
    GameInfoReq in{0x0042u, "speedrun-1"};
    protocol::Writer w;
    REQUIRE(encode(w, in).has_value());
    auto m = decode_client(w.view());
    REQUIRE(m.has_value());
    REQUIRE(std::get<GameInfoReq>(m.value()) == in);
}

TEST_CASE("d2cs: GameInfoReply round-trip (3 players)", "[protocol][d2cs]") {
    GameInfoReply in;
    in.seqno     = 0x0042u;
    in.gameflag  = 0x00000004u;
    in.etime     = 0x00001234u;
    in.charlevel = 25u;
    in.leveldiff = 5u;
    in.maxchar   = 8u;
    in.currchar  = 3u;
    in.chclass    = {1,2,3, 0,0,0,0,0,0,0,0,0,0,0,0,0};
    in.charlevels = {25,24,26, 0,0,0,0,0,0,0,0,0,0,0,0,0};
    in.game_desc  = "Diff: Normal";
    in.char_names = {"Alice", "Bob", "Charlie"};
    protocol::Writer w;
    REQUIRE(encode(w, in).has_value());
    auto m = decode_server(w.view());
    REQUIRE(m.has_value());
    REQUIRE(std::get<GameInfoReply>(m.value()) == in);
}

TEST_CASE("d2cs: GameInfoReply rejects oversize currchar", "[protocol][d2cs]") {
    // Craft a malformed reply manually: claim currchar=17 (over the 16 cap).
    protocol::Writer w;
    protocol::Writer p;
    p.write_le<std::uint16_t>(0u);              // seqno
    p.write_le<std::uint32_t>(0u);              // gameflag
    p.write_le<std::uint32_t>(0u);              // etime
    p.write_le<std::uint8_t>(0u);               // charlevel
    p.write_le<std::uint8_t>(0u);               // leveldiff
    p.write_le<std::uint8_t>(0u);               // maxchar
    p.write_le<std::uint8_t>(17u);              // currchar > 16
    for (int i = 0; i < 16; ++i) p.write_le<std::uint8_t>(0u);
    for (int i = 0; i < 16; ++i) p.write_le<std::uint8_t>(0u);
    p.write_cstring("");                        // game_desc
    // Emit with the same framing as the codec.
    const auto body = p.view();
    const auto total = body.size() + 3u;  // D2csHeader::kSize
    w.write_le<std::uint16_t>(static_cast<std::uint16_t>(total));
    w.write_le<std::uint8_t>(0x06u);            // GAMEINFOREPLY
    w.write_bytes(body);
    auto r = decode_server(w.view());
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error().code() == core::StatusCode::InvalidArgument);
}

TEST_CASE("d2cs: CharListReq round-trip", "[protocol][d2cs]") {
    CharListReq in{8u, 0u};
    protocol::Writer w;
    REQUIRE(encode(w, in).has_value());
    auto m = decode_client(w.view());
    REQUIRE(m.has_value());
    REQUIRE(std::get<CharListReq>(m.value()) == in);
}

TEST_CASE("d2cs: CharListReply round-trip (2 characters)", "[protocol][d2cs]") {
    CharListReply in;
    in.maxchar   = 8u;
    in.currchar  = 2u;
    in.u1        = 0u;
    in.currchar2 = 2u;
    CharListEntry e1;
    e1.name = "Alice";
    for (std::size_t i = 0; i < e1.portrait.size(); ++i)
        e1.portrait[i] = static_cast<std::uint8_t>(i);
    CharListEntry e2;
    e2.name = "Bob";
    for (std::size_t i = 0; i < e2.portrait.size(); ++i)
        e2.portrait[i] = static_cast<std::uint8_t>(0xFF - i);
    in.chars = {e1, e2};
    protocol::Writer w;
    REQUIRE(encode(w, in).has_value());
    auto m = decode_server(w.view());
    REQUIRE(m.has_value());
    REQUIRE(std::get<CharListReply>(m.value()) == in);
}
