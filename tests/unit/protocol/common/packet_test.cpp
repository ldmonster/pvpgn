// SPDX-License-Identifier: GPL-2.0-or-later
#include <array>
#include <cstddef>

#include <catch2/catch_test_macros.hpp>

#include "core/bytes.hpp"
#include "protocol/common/packet.hpp"

using namespace pvpgn;

namespace {

constexpr core::ByteView bytes_of(const std::byte* p, std::size_t n) {
    return core::ByteView{p, n};
}

}  // namespace

TEST_CASE("parse_bnet_header reads marker/code/size", "[protocol][packet]") {
    constexpr std::array raw = {
        std::byte{0xFF}, std::byte{0x50}, std::byte{0x09}, std::byte{0x00},
    };
    auto h = protocol::parse_bnet_header(bytes_of(raw.data(), raw.size()));
    REQUIRE(h.has_value());
    REQUIRE(h.value().marker == 0xFFu);
    REQUIRE(h.value().code   == 0x50u);
    REQUIRE(h.value().size   == 0x0009u);
}

TEST_CASE("parse_bnet_header rejects short buffer", "[protocol][packet]") {
    constexpr std::array raw = {std::byte{0xFF}, std::byte{0x50}};
    auto h = protocol::parse_bnet_header(bytes_of(raw.data(), raw.size()));
    REQUIRE_FALSE(h.has_value());
    REQUIRE(h.error().code() == core::StatusCode::OutOfRange);
}

TEST_CASE("parse_bnet_header rejects bad marker", "[protocol][packet]") {
    constexpr std::array raw = {
        std::byte{0xAA}, std::byte{0x50}, std::byte{0x04}, std::byte{0x00},
    };
    auto h = protocol::parse_bnet_header(bytes_of(raw.data(), raw.size()));
    REQUIRE_FALSE(h.has_value());
    REQUIRE(h.error().code() == core::StatusCode::InvalidArgument);
}

TEST_CASE("parse_bnet_header rejects size < header", "[protocol][packet]") {
    constexpr std::array raw = {
        std::byte{0xFF}, std::byte{0x50}, std::byte{0x03}, std::byte{0x00},
    };
    auto h = protocol::parse_bnet_header(bytes_of(raw.data(), raw.size()));
    REQUIRE_FALSE(h.has_value());
    REQUIRE(h.error().code() == core::StatusCode::InvalidArgument);
}

TEST_CASE("write_bnet_header round-trips", "[protocol][packet]") {
    std::array<std::byte, 4> out{};
    protocol::BnetHeader h{protocol::kBnetMarker, 0x0E, 0x1234};
    auto s = protocol::write_bnet_header(core::ByteSpan{out}, h);
    REQUIRE(s.has_value());
    auto parsed = protocol::parse_bnet_header(core::ByteView{out});
    REQUIRE(parsed.has_value());
    REQUIRE(parsed.value() == h);
}

TEST_CASE("parse_packet frames one full packet", "[protocol][packet]") {
    // marker=0xFF code=0x25 size=0x0008 payload= 01 02 03 04
    constexpr std::array raw = {
        std::byte{0xFF}, std::byte{0x25}, std::byte{0x08}, std::byte{0x00},
        std::byte{0x01}, std::byte{0x02}, std::byte{0x03}, std::byte{0x04},
    };
    auto fp = protocol::parse_packet(bytes_of(raw.data(), raw.size()));
    REQUIRE(fp.has_value());
    REQUIRE(fp.value().consumed              == 8);
    REQUIRE(fp.value().packet.header.code    == 0x25);
    REQUIRE(fp.value().packet.payload.size() == 4);
    REQUIRE(static_cast<std::uint8_t>(fp.value().packet.payload[0]) == 0x01);
    REQUIRE(static_cast<std::uint8_t>(fp.value().packet.payload[3]) == 0x04);
}

TEST_CASE("parse_packet reports incomplete buffer", "[protocol][packet]") {
    // header says size=8 but only 6 bytes available
    constexpr std::array raw = {
        std::byte{0xFF}, std::byte{0x25}, std::byte{0x08}, std::byte{0x00},
        std::byte{0x01}, std::byte{0x02},
    };
    auto fp = protocol::parse_packet(bytes_of(raw.data(), raw.size()));
    REQUIRE_FALSE(fp.has_value());
    REQUIRE(fp.error().code() == core::StatusCode::OutOfRange);
}
