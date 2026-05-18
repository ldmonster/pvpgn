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

TEST_CASE("Writer write_string_no_nul mirrors legacy packet_append_ntstring",
          "[protocol][writer]") {
    protocol::Writer w;
    w.write_string_no_nul("ABCD");
    auto bytes = w.view();
    REQUIRE(bytes.size() == 4);
    REQUIRE(static_cast<std::uint8_t>(bytes[0]) == 'A');
    REQUIRE(static_cast<std::uint8_t>(bytes[3]) == 'D');

    // Compared to write_cstring (which DOES add a NUL):
    protocol::Writer w2;
    w2.write_cstring("ABCD");
    REQUIRE(w2.view().size() == 5);
    REQUIRE(static_cast<std::uint8_t>(w2.view()[4]) == 0u);

    // Empty string is a no-op.
    protocol::Writer w3;
    w3.write_string_no_nul("");
    REQUIRE(w3.empty());
}

TEST_CASE("Writer reserve + patch_* enables generic framing",
          "[protocol][writer]") {
    // Simulate a d2cs-style 4-byte LE header: [u16 type] [u16 size].
    protocol::Writer w;
    const auto hdr_off = w.reserve(4);
    REQUIRE(hdr_off == 0);
    REQUIRE(w.size() == 4);

    // Body: one u32.
    w.write_le<std::uint32_t>(0xdeadbeef);

    // Back-patch fields.
    REQUIRE(w.patch_le<std::uint16_t>(hdr_off + 0, 0x1234).has_value());
    REQUIRE(w.patch_le<std::uint16_t>(
        hdr_off + 2, static_cast<std::uint16_t>(w.size())).has_value());

    const auto bytes = w.view();
    REQUIRE(bytes.size() == 8);
    REQUIRE(static_cast<std::uint8_t>(bytes[0]) == 0x34);  // type LE lo
    REQUIRE(static_cast<std::uint8_t>(bytes[1]) == 0x12);
    REQUIRE(static_cast<std::uint8_t>(bytes[2]) == 0x08);  // size = 8
    REQUIRE(static_cast<std::uint8_t>(bytes[3]) == 0x00);
    REQUIRE(static_cast<std::uint8_t>(bytes[4]) == 0xef);
    REQUIRE(static_cast<std::uint8_t>(bytes[7]) == 0xde);
}

TEST_CASE("Writer patch_* bounds checks", "[protocol][writer]") {
    protocol::Writer w;
    w.write_u8(0);
    w.write_u8(0);

    // In-bounds u8.
    REQUIRE(w.patch_u8(0, 0xAB).has_value());
    REQUIRE(static_cast<std::uint8_t>(w.view()[0]) == 0xAB);

    // Out-of-bounds u8.
    auto s1 = w.patch_u8(2, 0);
    REQUIRE_FALSE(s1.has_value());
    REQUIRE(s1.error().code() == core::StatusCode::OutOfRange);

    // Out-of-bounds u32 (would need 4 bytes; only 2 available).
    auto s2 = w.patch_le<std::uint32_t>(0, 0);
    REQUIRE_FALSE(s2.has_value());
    REQUIRE(s2.error().code() == core::StatusCode::OutOfRange);
}

TEST_CASE("Writer reserve + patch lets bnet framing be expressed manually",
          "[protocol][writer]") {
    // Parity exercise: build the same bytes as begin_bnet_packet/finalize
    // using only reserve+patch, to demonstrate the generic primitive is
    // sufficient for protocols whose header layout differs from BNet.
    protocol::Writer manual;
    const auto hdr = manual.reserve(protocol::BnetHeader::kSize);
    manual.write_le<std::uint32_t>(0u);
    manual.write_cstring("AllSeeingEye");
    REQUIRE(manual.patch_u8(hdr + 0, protocol::kBnetMarker).has_value());
    REQUIRE(manual.patch_u8(hdr + 1, 0x0E).has_value());
    REQUIRE(manual.patch_le<std::uint16_t>(
        hdr + 2, static_cast<std::uint16_t>(manual.size())).has_value());

    protocol::Writer baseline;
    baseline.begin_bnet_packet(0x0E);
    baseline.write_le<std::uint32_t>(0u);
    baseline.write_cstring("AllSeeingEye");
    REQUIRE(baseline.finalize_bnet_packet().has_value());

    const auto a = manual.view();
    const auto b = baseline.view();
    REQUIRE(a.size() == b.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
        REQUIRE(static_cast<std::uint8_t>(a[i]) ==
                static_cast<std::uint8_t>(b[i]));
    }
}
