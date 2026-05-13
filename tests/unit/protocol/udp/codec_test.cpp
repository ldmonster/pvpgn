// SPDX-License-Identifier: GPL-2.0-or-later
#include <array>
#include <variant>

#include <catch2/catch_test_macros.hpp>

#include "protocol/udp/codec.hpp"

using namespace pvpgn;
using namespace pvpgn::protocol::udp;

namespace {
template <class T>
Datagram round_trip(const Datagram& dg) {
    protocol::Writer w;
    REQUIRE(encode(w, dg).has_value());
    auto out = decode(w.view());
    REQUIRE(out.has_value());
    REQUIRE(std::holds_alternative<T>(out.value()));
    return out.value();
}
}  // namespace

TEST_CASE("udp: UdpTest round-trip", "[protocol][udp]") {
    auto r = round_trip<UdpTest>(Datagram{UdpTest{kBnetTag}});
    REQUIRE(std::get<UdpTest>(r).bnettag == kBnetTag);
}

TEST_CASE("udp: UdpPing round-trip", "[protocol][udp]") {
    auto r = round_trip<UdpPing>(Datagram{UdpPing{0x12345678u}});
    REQUIRE(std::get<UdpPing>(r).cookie == 0x12345678u);
}

TEST_CASE("udp: SessionAddr1 round-trip", "[protocol][udp]") {
    auto r = round_trip<SessionAddr1>(Datagram{SessionAddr1{0xDEADBEEFu}});
    REQUIRE(std::get<SessionAddr1>(r).session_key == 0xDEADBEEFu);
}

TEST_CASE("udp: SessionAddr2 round-trip", "[protocol][udp]") {
    SessionAddr2 in{0xAABBCCDDu, 0x11223344u};
    auto r = round_trip<SessionAddr2>(Datagram{in});
    REQUIRE(std::get<SessionAddr2>(r) == in);
}

TEST_CASE("udp: unknown type returns Unimplemented", "[protocol][udp]") {
    std::array<std::byte, 8> raw{
        std::byte{0x99}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
    auto r = decode(core::ByteView{raw});
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error().code() == core::StatusCode::Unimplemented);
}

TEST_CASE("udp: short buffer returns OutOfRange", "[protocol][udp]") {
    std::array<std::byte, 2> raw{std::byte{0x07}, std::byte{0x00}};
    auto r = decode(core::ByteView{raw});
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error().code() == core::StatusCode::OutOfRange);
}
