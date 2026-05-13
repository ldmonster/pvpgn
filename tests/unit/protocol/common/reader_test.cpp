// SPDX-License-Identifier: GPL-2.0-or-later
#include <array>
#include <cstddef>
#include <string>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "protocol/common/reader.hpp"

using namespace pvpgn;

TEST_CASE("Reader reads LE integers and advances", "[protocol][reader]") {
    constexpr std::array raw = {
        std::byte{0x78}, std::byte{0x56},                                   // u16 = 0x5678
        std::byte{0xEF}, std::byte{0xCD}, std::byte{0xAB}, std::byte{0x89}, // u32
        std::byte{0x42},                                                    // u8
    };
    protocol::Reader r{core::ByteView{raw}};
    REQUIRE(r.size()      == 7);
    REQUIRE(r.remaining() == 7);

    auto v16 = r.read_le<std::uint16_t>();
    REQUIRE(v16.has_value());
    REQUIRE(v16.value() == 0x5678);

    auto v32 = r.read_le<std::uint32_t>();
    REQUIRE(v32.has_value());
    REQUIRE(v32.value() == 0x89ABCDEFu);

    auto v8 = r.read_le<std::uint8_t>();
    REQUIRE(v8.has_value());
    REQUIRE(v8.value() == 0x42);
    REQUIRE(r.empty());
}

TEST_CASE("Reader OOB does not advance cursor", "[protocol][reader]") {
    constexpr std::array raw = {std::byte{0x01}, std::byte{0x02}};
    protocol::Reader r{core::ByteView{raw}};
    auto v = r.read_le<std::uint32_t>();
    REQUIRE_FALSE(v.has_value());
    REQUIRE(v.error().code() == core::StatusCode::OutOfRange);
    REQUIRE(r.position() == 0);  // not advanced
}

TEST_CASE("Reader read_cstring returns view to NUL-terminated string",
          "[protocol][reader]") {
    constexpr std::array raw = {
        std::byte{'a'}, std::byte{'l'}, std::byte{'i'}, std::byte{'c'},
        std::byte{'e'}, std::byte{0x00},
        std::byte{'!'},
    };
    protocol::Reader r{core::ByteView{raw}};
    auto s = r.read_cstring();
    REQUIRE(s.has_value());
    REQUIRE(std::string{s.value()} == "alice");
    REQUIRE(r.remaining() == 1);
    auto rest = r.read_le<std::uint8_t>();
    REQUIRE(rest.has_value());
    REQUIRE(rest.value() == '!');
}

TEST_CASE("Reader unterminated string returns OutOfRange",
          "[protocol][reader]") {
    constexpr std::array raw = {
        std::byte{'h'}, std::byte{'i'}, std::byte{'!'},
    };
    protocol::Reader r{core::ByteView{raw}};
    auto s = r.read_cstring();
    REQUIRE_FALSE(s.has_value());
    REQUIRE(s.error().code() == core::StatusCode::OutOfRange);
}

TEST_CASE("Reader read_bytes returns subspan", "[protocol][reader]") {
    constexpr std::array raw = {
        std::byte{0xAA}, std::byte{0xBB}, std::byte{0xCC}, std::byte{0xDD},
    };
    protocol::Reader r{core::ByteView{raw}};
    auto blob = r.read_bytes(3);
    REQUIRE(blob.has_value());
    REQUIRE(blob.value().size() == 3);
    REQUIRE(static_cast<std::uint8_t>(blob.value()[0]) == 0xAA);
    REQUIRE(static_cast<std::uint8_t>(blob.value()[2]) == 0xCC);
    REQUIRE(r.remaining() == 1);
}

TEST_CASE("Reader skip advances cursor; OOB rejected",
          "[protocol][reader]") {
    constexpr std::array raw = {
        std::byte{0x01}, std::byte{0x02}, std::byte{0x03}, std::byte{0x04},
    };
    protocol::Reader r{core::ByteView{raw}};
    REQUIRE(r.skip(2).has_value());
    REQUIRE(r.position() == 2);
    auto err = r.skip(99);
    REQUIRE_FALSE(err.has_value());
    REQUIRE(err.error().code() == core::StatusCode::OutOfRange);
    REQUIRE(r.position() == 2);
}

TEST_CASE("Reader read_be handles big-endian", "[protocol][reader]") {
    constexpr std::array raw = {
        std::byte{0x12}, std::byte{0x34}, std::byte{0x56}, std::byte{0x78},
    };
    protocol::Reader r{core::ByteView{raw}};
    auto v = r.read_be<std::uint32_t>();
    REQUIRE(v.has_value());
    REQUIRE(v.value() == 0x12345678u);
}
