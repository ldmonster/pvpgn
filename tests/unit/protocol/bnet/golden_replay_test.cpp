// SPDX-License-Identifier: GPL-2.0-or-later
// Golden replay tests: decode known wire bytes and verify the resulting
// message structs.  These catch regressions in codec implementations.

#include <array>
#include <cstddef>
#include <cstdint>
#include <variant>

#include <catch2/catch_test_macros.hpp>

#include "protocol/bnet/codec.hpp"
#include "protocol/bnet/messages.hpp"
#include "protocol/common/packet.hpp"

using namespace pvpgn;

namespace pvpgn::protocol::bnet::test {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/// Convert a fixed-size uint8_t array to a core::ByteView (span<const byte>).
template <std::size_t N>
static core::ByteView as_byte_view(const std::array<std::uint8_t, N>& arr) {
    return std::as_bytes(std::span<const std::uint8_t, N>{arr});
}

// ---------------------------------------------------------------------------
// SID_NULL (0x00) — client direction
// ---------------------------------------------------------------------------

TEST_CASE("BNetGoldenReplay/SID_NULL_Packet", "[protocol][bnet]") {
    // Known good wire bytes for SID_NULL:
    //   [0xFF, 0x00, 0x04, 0x00]
    //   marker=0xFF  code=0x00  size=4 (LE)  payload=<empty>
    const std::array<std::uint8_t, 4> raw = {0xFF, 0x00, 0x04, 0x00};

    // Verify raw byte layout (golden assertion).
    CHECK(raw[0] == 0xFF);  // BNet packet marker
    CHECK(raw[1] == 0x00);  // SID_NULL
    CHECK(raw[2] == 0x04);  // size low byte
    CHECK(raw[3] == 0x00);  // size high byte

    // Parse the frame.
    auto fp = protocol::parse_packet(as_byte_view(raw));
    REQUIRE(fp.has_value());
    CHECK(fp.value().consumed == 4);
    CHECK(fp.value().packet.header.code == kSidNull);

    // Decode as a client message.
    auto result = decode_client(fp.value().packet);
    REQUIRE(result.has_value());
    REQUIRE(std::holds_alternative<Null>(result.value()));
}

// ---------------------------------------------------------------------------
// SID_NULL (0x00) — server direction
// ---------------------------------------------------------------------------

TEST_CASE("BNetGoldenReplay/SID_NULL_Server", "[protocol][bnet]") {
    const std::array<std::uint8_t, 4> raw = {0xFF, 0x00, 0x04, 0x00};

    auto fp = protocol::parse_packet(as_byte_view(raw));
    REQUIRE(fp.has_value());

    auto result = decode_server(fp.value().packet);
    REQUIRE(result.has_value());
    REQUIRE(std::holds_alternative<Null>(result.value()));
}

// ---------------------------------------------------------------------------
// SID_PING (0x25) — client direction, cookie 0xDEADBEEF
// ---------------------------------------------------------------------------

TEST_CASE("BNetGoldenReplay/SID_PING_Packet", "[protocol][bnet]") {
    // Known good wire bytes for SID_PING with cookie 0xDEADBEEF (LE):
    //   [0xFF, 0x25, 0x08, 0x00, 0xEF, 0xBE, 0xAD, 0xDE]
    const std::array<std::uint8_t, 8> raw = {
        0xFF, 0x25, 0x08, 0x00,
        0xEF, 0xBE, 0xAD, 0xDE  // cookie 0xDEADBEEF in little-endian
    };

    // Verify raw byte layout (golden assertion).
    CHECK(raw[0] == 0xFF);  // BNet packet marker
    CHECK(raw[1] == 0x25);  // SID_PING
    CHECK(raw[2] == 0x08);  // size low byte
    CHECK(raw[3] == 0x00);  // size high byte
    CHECK(raw[4] == 0xEF);  // cookie byte 0 (LSB)
    CHECK(raw[5] == 0xBE);  // cookie byte 1
    CHECK(raw[6] == 0xAD);  // cookie byte 2
    CHECK(raw[7] == 0xDE);  // cookie byte 3 (MSB)

    // Parse the frame.
    auto fp = protocol::parse_packet(as_byte_view(raw));
    REQUIRE(fp.has_value());
    CHECK(fp.value().consumed == 8);
    CHECK(fp.value().packet.header.code == kSidPing);

    // Decode as a client message.
    auto result = decode_client(fp.value().packet);
    REQUIRE(result.has_value());
    REQUIRE(std::holds_alternative<Ping>(result.value()));
    CHECK(std::get<Ping>(result.value()).ticks == 0xDEADBEEFu);
}

// ---------------------------------------------------------------------------
// SID_PING (0x25) — server direction, cookie 0xDEADBEEF
// ---------------------------------------------------------------------------

TEST_CASE("BNetGoldenReplay/SID_PING_Server", "[protocol][bnet]") {
    const std::array<std::uint8_t, 8> raw = {
        0xFF, 0x25, 0x08, 0x00,
        0xEF, 0xBE, 0xAD, 0xDE
    };

    auto fp = protocol::parse_packet(as_byte_view(raw));
    REQUIRE(fp.has_value());

    auto result = decode_server(fp.value().packet);
    REQUIRE(result.has_value());
    REQUIRE(std::holds_alternative<Ping>(result.value()));
    CHECK(std::get<Ping>(result.value()).ticks == 0xDEADBEEFu);
}

// ---------------------------------------------------------------------------
// SID_PING (0x25) — round-trip, cookie 0x12345678
// ---------------------------------------------------------------------------

TEST_CASE("BNetGoldenReplay/RoundTrip_SID_NULL", "[protocol][bnet]") {
    const std::array<std::uint8_t, 4> raw = {0xFF, 0x00, 0x04, 0x00};

    auto fp = protocol::parse_packet(as_byte_view(raw));
    REQUIRE(fp.has_value());
    CHECK(fp.value().packet.header.marker == protocol::kBnetMarker);
    CHECK(fp.value().packet.header.code   == kSidNull);
    CHECK(fp.value().packet.header.size   == 4);
    CHECK(fp.value().packet.payload.empty());

    auto result = decode_client(fp.value().packet);
    REQUIRE(result.has_value());
    REQUIRE(std::holds_alternative<Null>(result.value()));
}

TEST_CASE("BNetGoldenReplay/RoundTrip_SID_PING", "[protocol][bnet]") {
    // cookie 0x12345678 in little-endian: 0x78, 0x56, 0x34, 0x12
    const std::array<std::uint8_t, 8> raw = {
        0xFF, 0x25, 0x08, 0x00,
        0x78, 0x56, 0x34, 0x12
    };

    auto fp = protocol::parse_packet(as_byte_view(raw));
    REQUIRE(fp.has_value());
    CHECK(fp.value().packet.header.marker == protocol::kBnetMarker);
    CHECK(fp.value().packet.header.code   == kSidPing);
    CHECK(fp.value().packet.header.size   == 8);
    CHECK(fp.value().packet.payload.size() == 4);

    auto result = decode_client(fp.value().packet);
    REQUIRE(result.has_value());
    REQUIRE(std::holds_alternative<Ping>(result.value()));
    CHECK(std::get<Ping>(result.value()).ticks == 0x12345678u);
}

}  // namespace pvpgn::protocol::bnet::test
