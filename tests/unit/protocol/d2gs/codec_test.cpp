// SPDX-License-Identifier: GPL-2.0-or-later
#include <variant>

#include <catch2/catch_test_macros.hpp>

#include "protocol/d2gs/codec.hpp"

using namespace pvpgn;
using namespace pvpgn::protocol::d2gs;

TEST_CASE("d2gs: SetGsInfo round-trip (both directions)", "[protocol][d2gs]") {
    SetGsInfo in{/*seqno*/ 0xDEADu, /*max*/ 32u, /*flag*/ 0x01u};
    protocol::Writer w;
    REQUIRE(encode(w, in).has_value());

    auto down = decode_d2cs_to_d2gs(w.view());
    REQUIRE(down.has_value());
    REQUIRE(std::get<SetGsInfo>(down.value()) == in);

    auto up = decode_d2gs_to_d2cs(w.view());
    REQUIRE(up.has_value());
    REQUIRE(std::get<SetGsInfo>(up.value()) == in);
}

TEST_CASE("d2gs: EchoReq down / EchoReply up share wire", "[protocol][d2gs]") {
    EchoReq   req{42};
    EchoReply rep{42};
    protocol::Writer w1, w2;
    REQUIRE(encode(w1, req).has_value());
    REQUIRE(encode(w2, rep).has_value());

    auto d = decode_d2cs_to_d2gs(w1.view());
    REQUIRE(d.has_value());
    REQUIRE(std::get<EchoReq>(d.value()) == req);

    auto u = decode_d2gs_to_d2cs(w2.view());
    REQUIRE(u.has_value());
    REQUIRE(std::get<EchoReply>(u.value()) == rep);
}

TEST_CASE("d2gs: Control round-trip", "[protocol][d2gs]") {
    Control in{/*seqno*/ 7u, kControlShutdown, 0u};
    protocol::Writer w;
    REQUIRE(encode(w, in).has_value());
    auto m = decode_d2cs_to_d2gs(w.view());
    REQUIRE(m.has_value());
    REQUIRE(std::get<Control>(m.value()) == in);
}

TEST_CASE("d2gs: Control rejected on upstream direction",
          "[protocol][d2gs]") {
    Control in{1u, kControlRestart, 0u};
    protocol::Writer w;
    REQUIRE(encode(w, in).has_value());
    auto m = decode_d2gs_to_d2cs(w.view());
    REQUIRE_FALSE(m.has_value());
    REQUIRE(m.error().code() == core::StatusCode::Unimplemented);
}

TEST_CASE("d2gs: parse_header rejects size < header", "[protocol][d2gs]") {
    std::array<std::byte, 8> raw{
        std::byte{0x04}, std::byte{0x00},
        std::byte{0x12}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
    auto r = parse_header(core::ByteView{raw});
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error().code() == core::StatusCode::InvalidArgument);
}

TEST_CASE("d2gs: DownAuthReq round-trip", "[protocol][d2gs]") {
    DownAuthReq in{};
    in.seqno = 0x42u;
    in.session_num = 0x10u;
    in.realm_name = "PvPGN";
    in.key_checksum = {std::byte{0xCA}, std::byte{0xFE}, std::byte{0xBA},
                       std::byte{0xBE}};
    protocol::Writer w;
    REQUIRE(encode(w, in).has_value());
    auto m = decode_d2cs_to_d2gs(w.view());
    REQUIRE(m.has_value());
    REQUIRE(std::get<DownAuthReq>(m.value()) == in);
}

TEST_CASE("d2gs: DownAuthReply round-trip (verdict)", "[protocol][d2gs]") {
    DownAuthReply in{0x10u, kAuthReplyBadVersion};
    protocol::Writer w;
    REQUIRE(encode(w, in).has_value());
    auto m = decode_d2cs_to_d2gs(w.view());
    REQUIRE(m.has_value());
    REQUIRE(std::get<DownAuthReply>(m.value()) == in);
}

TEST_CASE("d2gs: UpAuthReply round-trip (signed reply)",
          "[protocol][d2gs]") {
    UpAuthReply in{};
    in.seqno = 0x11u;
    in.version = 0xC0DEu;
    in.checksum = 0xBABEu;
    in.randnum = 0x12345678u;
    in.signlen = 64;
    for (std::size_t i = 0; i < in.sign.size(); ++i) {
        in.sign[i] = std::byte{static_cast<std::uint8_t>(i & 0xFF)};
    }
    protocol::Writer w;
    REQUIRE(encode(w, in).has_value());
    auto m = decode_d2gs_to_d2cs(w.view());
    REQUIRE(m.has_value());
    REQUIRE(std::get<UpAuthReply>(m.value()) == in);
}

TEST_CASE("d2gs: 0x11 disambiguated by direction", "[protocol][d2gs]") {
    // A DownAuthReply (4-byte body) must NOT be decodable as an
    // UpAuthReply (which needs 4*u32 + 128 sign bytes).
    DownAuthReply in{1u, kAuthReplyOk};
    protocol::Writer w;
    REQUIRE(encode(w, in).has_value());
    auto bad = decode_d2gs_to_d2cs(w.view());
    REQUIRE_FALSE(bad.has_value());
    REQUIRE(bad.error().code() == core::StatusCode::OutOfRange);
}
