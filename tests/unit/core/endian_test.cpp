// SPDX-License-Identifier: GPL-2.0-or-later
#include <array>
#include <cstddef>
#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "core/endian.hpp"

using namespace pvpgn::core;

TEST_CASE("read_le<uint32_t>", "[core][endian]") {
    std::array<std::byte, 4> buf{std::byte{0x78}, std::byte{0x56},
                                 std::byte{0x34}, std::byte{0x12}};
    auto r = read_le<std::uint32_t>(buf);
    REQUIRE(r);
    REQUIRE(r.value() == 0x12345678U);
}

TEST_CASE("read_be<uint32_t>", "[core][endian]") {
    std::array<std::byte, 4> buf{std::byte{0x12}, std::byte{0x34},
                                 std::byte{0x56}, std::byte{0x78}};
    auto r = read_be<std::uint32_t>(buf);
    REQUIRE(r);
    REQUIRE(r.value() == 0x12345678U);
}

TEST_CASE("write_le<uint16_t>", "[core][endian]") {
    std::array<std::byte, 2> buf{};
    REQUIRE(write_le<std::uint16_t>(buf, 0xABCD));
    REQUIRE(static_cast<unsigned>(buf[0]) == 0xCD);
    REQUIRE(static_cast<unsigned>(buf[1]) == 0xAB);
}

TEST_CASE("write_be<uint64_t>", "[core][endian]") {
    std::array<std::byte, 8> buf{};
    REQUIRE(write_be<std::uint64_t>(buf, 0x0102030405060708ULL));
    REQUIRE(static_cast<unsigned>(buf[0]) == 0x01);
    REQUIRE(static_cast<unsigned>(buf[7]) == 0x08);
}

TEST_CASE("read_le short buffer => OutOfRange", "[core][endian]") {
    std::array<std::byte, 2> buf{};
    auto r = read_le<std::uint32_t>(buf);
    REQUIRE_FALSE(r);
    REQUIRE(r.error().code() == StatusCode::OutOfRange);
}

TEST_CASE("round-trip LE then read", "[core][endian]") {
    std::array<std::byte, 8> buf{};
    REQUIRE(write_le<std::uint64_t>(buf, 0xDEADBEEFCAFEBABEULL));
    auto r = read_le<std::uint64_t>(buf);
    REQUIRE(r);
    REQUIRE(r.value() == 0xDEADBEEFCAFEBABEULL);
}
