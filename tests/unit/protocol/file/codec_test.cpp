// SPDX-License-Identifier: GPL-2.0-or-later
#include <variant>

#include <catch2/catch_test_macros.hpp>

#include "protocol/file/codec.hpp"

using namespace pvpgn;
using namespace pvpgn::protocol::file;

TEST_CASE("file: ClientFileReq round-trip", "[protocol][file]") {
    ClientFileReq in{
        /*arch*/      0x49583836u,  // 'IX86'
        /*client*/    0x53455850u,  // 'SEXP'
        /*ad_id*/     0,
        /*ext*/       0,
        /*offset*/    0,
        /*timestamp*/ 0x01BC2809E1582C00ull,
        /*filename*/  "IX86ver1.mpq"};
    protocol::Writer w;
    REQUIRE(encode(w, in).has_value());

    auto m = decode(w.view());
    REQUIRE(m.has_value());
    REQUIRE(std::holds_alternative<ClientFileReq>(m.value()));
    REQUIRE(std::get<ClientFileReq>(m.value()) == in);
}

TEST_CASE("file: ServerFileReply round-trip", "[protocol][file]") {
    ServerFileReply in{
        /*file_len*/  0x00001B33u,
        /*ad_id*/     0,
        /*ext*/       0,
        /*timestamp*/ 0x01BC2809E1582C00ull,
        /*filename*/  "IX86ver1.mpq"};
    protocol::Writer w;
    REQUIRE(encode(w, in).has_value());
    auto m = decode(w.view());
    REQUIRE(m.has_value());
    REQUIRE(std::get<ServerFileReply>(m.value()) == in);
}

TEST_CASE("file: parse_header rejects size < header",
          "[protocol][file]") {
    std::array<std::byte, 4> raw{
        std::byte{0x02}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x01}};
    auto r = parse_header(core::ByteView{raw});
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error().code() == core::StatusCode::InvalidArgument);
}
