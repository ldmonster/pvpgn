// SPDX-License-Identifier: GPL-2.0-or-later
// Parity test for the v3 D2CS_CLIENT_CHARLISTREPLY encoder.

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "protocol/d2cs/charlistreply_encoder.hpp"

namespace clr = pvpgn::protocol::d2cs::charlistreply;

namespace {

std::uint16_t u16le(const std::vector<std::byte>& b, std::size_t off) {
    return static_cast<std::uint16_t>(
        static_cast<std::uint8_t>(b[off]) |
        (static_cast<std::uint8_t>(b[off + 1]) << 8));
}

clr::CharEntry make_entry(std::string name, std::string portrait_ascii) {
    clr::CharEntry e;
    e.charname = std::move(name);
    e.portrait.reserve(portrait_ascii.size());
    for (char c : portrait_ascii) {
        e.portrait.push_back(static_cast<std::byte>(static_cast<std::uint8_t>(c)));
    }
    return e;
}

}  // namespace

TEST_CASE("charlistreply: empty -> 11-byte base packet with zero counts",
          "[protocol][d2cs][charlistreply]") {
    auto bytes = clr::encode(/*maxchar_field=*/0, /*entries=*/{});
    REQUIRE(bytes.size() == clr::kReplyBaseSize);
    CHECK(u16le(bytes, 0) == 11);                             // size
    CHECK(static_cast<std::uint8_t>(bytes[2]) == clr::kPacketType);
    CHECK(u16le(bytes, 3) == 0);                              // maxchar
    CHECK(u16le(bytes, 5) == 0);                              // currchar
    CHECK(u16le(bytes, 7) == 0);                              // u1
    CHECK(u16le(bytes, 9) == 0);                              // currchar2
}

TEST_CASE("charlistreply: maxchar field passes through (allow_newchar=true)",
          "[protocol][d2cs][charlistreply]") {
    auto bytes = clr::encode(/*maxchar_field=*/18, /*entries=*/{});
    CHECK(u16le(bytes, 3) == 18);
}

TEST_CASE("charlistreply: single entry layout",
          "[protocol][d2cs][charlistreply]") {
    std::vector<clr::CharEntry> entries{ make_entry("Alice", "PORT") };
    auto bytes = clr::encode(/*maxchar_field=*/8, entries);

    // Base = 11; entry = "Alice\0" (6) + "PORT\0" (5) = 11; total = 22.
    REQUIRE(bytes.size() == 22u);
    CHECK(u16le(bytes, 0) == 22);
    CHECK(u16le(bytes, 3) == 8);   // maxchar
    CHECK(u16le(bytes, 5) == 1);   // currchar
    CHECK(u16le(bytes, 7) == 0);   // u1
    CHECK(u16le(bytes, 9) == 1);   // currchar2

    const char expected_tail[] = "Alice\0PORT";
    for (std::size_t i = 0; i < sizeof(expected_tail) - 1; ++i) {
        CHECK(static_cast<char>(bytes[11 + i]) == expected_tail[i]);
    }
    CHECK(static_cast<std::uint8_t>(bytes[21]) == 0u);  // portrait NUL
}

TEST_CASE("charlistreply: multiple entries preserve caller-supplied order",
          "[protocol][d2cs][charlistreply]") {
    std::vector<clr::CharEntry> entries{
        make_entry("Zed", "Z"),  // 4 + 2 = 6
        make_entry("Bob", "B"),  // 4 + 2 = 6
        make_entry("Al",  "A"),  // 3 + 2 = 5
    };
    auto bytes = clr::encode(/*maxchar_field=*/0, entries);

    // Body = 6 + 6 + 5 = 17; total = 28.
    REQUIRE(bytes.size() == 28u);
    CHECK(u16le(bytes, 5) == 3);   // currchar
    CHECK(u16le(bytes, 9) == 3);   // currchar2

    // Names should appear in the exact caller-supplied order.
    auto find = [&](std::size_t start, const char* needle) -> std::size_t {
        const std::size_t n = std::strlen(needle);
        for (std::size_t i = start; i + n <= bytes.size(); ++i) {
            bool match = true;
            for (std::size_t k = 0; k < n; ++k) {
                if (static_cast<char>(bytes[i + k]) != needle[k]) { match = false; break; }
            }
            if (match) return i;
        }
        return std::string::npos;
    };
    const auto a = find(11, "Zed");
    const auto b = find(a + 1, "Bob");
    const auto c = find(b + 1, "Al");
    CHECK(a != std::string::npos);
    CHECK(b != std::string::npos);
    CHECK(c != std::string::npos);
    CHECK(a < b);
    CHECK(b < c);
}

TEST_CASE("charlistreply: caller-supplied DESC order is preserved verbatim",
          "[protocol][d2cs][charlistreply]") {
    // Same data as above but DESC pre-sorted by caller.
    std::vector<clr::CharEntry> entries{
        make_entry("Al",  "A"),
        make_entry("Bob", "B"),
        make_entry("Zed", "Z"),
    };
    auto bytes = clr::encode(/*maxchar_field=*/0, entries);
    REQUIRE(bytes.size() == 28u);
    CHECK(static_cast<char>(bytes[11]) == 'A');
    CHECK(static_cast<char>(bytes[12]) == 'l');
    CHECK(static_cast<char>(bytes[13]) == 0);
}

TEST_CASE("charlistreply: portrait may contain non-printable bytes (no NUL)",
          "[protocol][d2cs][charlistreply]") {
    clr::CharEntry e;
    e.charname = "X";
    e.portrait = { std::byte{0x01}, std::byte{0xFF}, std::byte{0x80} };
    auto bytes = clr::encode(/*maxchar_field=*/1, { e });

    // Base 11 + "X\0" 2 + 3 portrait bytes + NUL = 17.
    REQUIRE(bytes.size() == 17u);
    CHECK(static_cast<char>(bytes[11]) == 'X');
    CHECK(static_cast<std::uint8_t>(bytes[12]) == 0u);
    CHECK(static_cast<std::uint8_t>(bytes[13]) == 0x01u);
    CHECK(static_cast<std::uint8_t>(bytes[14]) == 0xFFu);
    CHECK(static_cast<std::uint8_t>(bytes[15]) == 0x80u);
    CHECK(static_cast<std::uint8_t>(bytes[16]) == 0u);
}

TEST_CASE("charlistreply_110: type byte 0x19 and per-char expire_time prefix",
          "[protocol][d2cs][charlistreply]") {
    clr::CharEntry e;
    e.charname    = "X";
    e.portrait    = { std::byte{0xAB} };
    e.expire_time = 0x7FFFFFFFu;
    auto bytes = clr::encode_110(/*maxchar_field=*/1, { e });

    // Base 11 + expire_time 4 + "X\0" 2 + 1 portrait byte + portrait NUL 1 = 19.
    REQUIRE(bytes.size() == 19u);
    CHECK(static_cast<std::uint8_t>(bytes[2]) == clr::kPacketType110);  // 0x19
    CHECK(u16le(bytes, 0) == 19);                                       // size
    CHECK(u16le(bytes, 3) == 1);                                        // maxchar
    CHECK(u16le(bytes, 5) == 1);                                        // currchar
    CHECK(u16le(bytes, 7) == 0);                                        // u1
    CHECK(u16le(bytes, 9) == 1);                                        // currchar2
    // Per-char block: expire_time (LE u32) first, then name+NUL, portrait+NUL.
    CHECK(static_cast<std::uint8_t>(bytes[11]) == 0xFFu);
    CHECK(static_cast<std::uint8_t>(bytes[12]) == 0xFFu);
    CHECK(static_cast<std::uint8_t>(bytes[13]) == 0xFFu);
    CHECK(static_cast<std::uint8_t>(bytes[14]) == 0x7Fu);
    CHECK(static_cast<char>(bytes[15]) == 'X');
    CHECK(static_cast<std::uint8_t>(bytes[16]) == 0u);     // name NUL
    CHECK(static_cast<std::uint8_t>(bytes[17]) == 0xABu);  // portrait byte
    CHECK(static_cast<std::uint8_t>(bytes[18]) == 0u);     // portrait NUL
}

TEST_CASE("charlistreply_110: empty list still emits 0x19 base packet",
          "[protocol][d2cs][charlistreply]") {
    auto bytes = clr::encode_110(/*maxchar_field=*/0, /*entries=*/{});
    REQUIRE(bytes.size() == clr::kReplyBaseSize);
    CHECK(static_cast<std::uint8_t>(bytes[2]) == clr::kPacketType110);
    CHECK(u16le(bytes, 5) == 0);  // currchar
}
