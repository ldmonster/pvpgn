// SPDX-License-Identifier: GPL-2.0-or-later
#include <array>
#include <cstddef>
#include <string>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "protocol/common/packet.hpp"
#include "protocol/common/reader.hpp"
#include "protocol/common/writer.hpp"

using namespace pvpgn;

TEST_CASE("Writer builds a BNet packet and back-patches size",
          "[protocol][writer]") {
    protocol::Writer w;
    w.begin_bnet_packet(0x0E);          // SID_JOINCHANNEL
    w.write_le<std::uint32_t>(0);       // flags
    w.write_cstring("AllSeeingEye");
    REQUIRE(w.finalize_bnet_packet().has_value());

    const auto bytes = w.view();
    REQUIRE(bytes.size() == 4 + 4 + 13);

    auto fp = protocol::parse_packet(bytes);
    REQUIRE(fp.has_value());
    REQUIRE(fp.value().packet.header.code == 0x0E);
    REQUIRE(fp.value().packet.header.size == bytes.size());

    protocol::Reader r{fp.value().packet.payload};
    auto flags = r.read_le<std::uint32_t>();
    REQUIRE(flags.has_value());
    REQUIRE(flags.value() == 0u);
    auto name = r.read_cstring();
    REQUIRE(name.has_value());
    REQUIRE(std::string{name.value()} == "AllSeeingEye");
}

TEST_CASE("Writer finalize without begin returns FailedPrecondition",
          "[protocol][writer]") {
    protocol::Writer w;
    w.write_u8(0x00);
    auto s = w.finalize_bnet_packet();
    REQUIRE_FALSE(s.has_value());
    REQUIRE(s.error().code() == core::StatusCode::FailedPrecondition);
}

TEST_CASE("Writer LE / BE round-trip", "[protocol][writer]") {
    protocol::Writer w;
    w.write_le<std::uint32_t>(0x11223344);
    w.write_be<std::uint32_t>(0x11223344);

    auto bytes = w.view();
    REQUIRE(bytes.size() == 8);
    REQUIRE(static_cast<std::uint8_t>(bytes[0]) == 0x44);  // LE low
    REQUIRE(static_cast<std::uint8_t>(bytes[3]) == 0x11);  // LE high
    REQUIRE(static_cast<std::uint8_t>(bytes[4]) == 0x11);  // BE high
    REQUIRE(static_cast<std::uint8_t>(bytes[7]) == 0x44);  // BE low
}

TEST_CASE("Writer take() transfers buffer and resets",
          "[protocol][writer]") {
    protocol::Writer w;
    w.write_u8(0xAA);
    w.write_u8(0xBB);
    auto buf = w.take();
    REQUIRE(buf.size() == 2);
    REQUIRE(w.empty());
}
