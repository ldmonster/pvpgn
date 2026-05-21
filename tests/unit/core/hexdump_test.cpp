// SPDX-License-Identifier: GPL-2.0-or-later
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "core/hexdump.hpp"

using namespace pvpgn::core;

// ---------------------------------------------------------------------------
// hexdump_line — single row formatting
// ---------------------------------------------------------------------------

TEST_CASE("hexdump_line: full 16-byte row at offset 0", "[core][hexdump]") {
    // 16 bytes: 0x00..0x0F
    const std::uint8_t data[16] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f
    };
    const auto line = hexdump_line(data, 16, 0);

    // Address column
    REQUIRE(line.substr(0, 4) == "0000");
    REQUIRE(line[4] == ':');

    // Hex bytes present
    REQUIRE(line.find("00 ") != std::string::npos);
    REQUIRE(line.find("0f ") != std::string::npos);

    // ASCII column: all non-printable → all dots
    const auto ascii_start = line.rfind("   ") + 3;
    const auto ascii_part = line.substr(ascii_start);
    REQUIRE(ascii_part.size() == 16);
    for (char c : ascii_part) {
        REQUIRE(c == '.');
    }
}

TEST_CASE("hexdump_line: printable ASCII bytes appear in ASCII column", "[core][hexdump]") {
    // 'A' = 0x41, 'B' = 0x42, ... 'P' = 0x50
    const std::uint8_t data[16] = {
        'A','B','C','D','E','F','G','H',
        'I','J','K','L','M','N','O','P'
    };
    const auto line = hexdump_line(data, 16, 0);

    // The ASCII column should end with "ABCDEFGHIJKLMNOP"
    REQUIRE(line.size() >= 16);
    const auto tail = line.substr(line.size() - 16);
    REQUIRE(tail == "ABCDEFGHIJKLMNOP");
}

TEST_CASE("hexdump_line: short row (< 16 bytes) pads hex with spaces", "[core][hexdump]") {
    const std::uint8_t data[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    const auto line = hexdump_line(data, 4, 0);

    // Must contain the 4 hex bytes
    REQUIRE(line.find("de ") != std::string::npos);
    REQUIRE(line.find("ad ") != std::string::npos);
    REQUIRE(line.find("be ") != std::string::npos);
    REQUIRE(line.find("ef ") != std::string::npos);

    // ASCII column: all non-printable → dots
    const auto ascii_start = line.rfind("   ") + 3;
    const auto ascii_part = line.substr(ascii_start);
    REQUIRE(ascii_part.size() == 4);
    for (char c : ascii_part) {
        REQUIRE(c == '.');
    }
}

TEST_CASE("hexdump_line: offset appears in address column", "[core][hexdump]") {
    const std::uint8_t data[1] = {0x42};
    // offset = 0x0010 → address column should be "0010"
    const auto line = hexdump_line(data, 1, 0x0010);
    REQUIRE(line.substr(0, 4) == "0010");
}

TEST_CASE("hexdump_line: offset 0x00FF", "[core][hexdump]") {
    const std::uint8_t data[1] = {0x00};
    const auto line = hexdump_line(data, 1, 0x00FF);
    REQUIRE(line.substr(0, 4) == "00ff");
}

// ---------------------------------------------------------------------------
// hexdump — full buffer formatting
// ---------------------------------------------------------------------------

TEST_CASE("hexdump: empty buffer returns empty string", "[core][hexdump]") {
    REQUIRE(hexdump(nullptr, 0).empty());
    REQUIRE(hexdump(static_cast<const void*>(nullptr), 0).empty());
}

TEST_CASE("hexdump: null pointer returns empty string", "[core][hexdump]") {
    REQUIRE(hexdump(nullptr, 16).empty());
}

TEST_CASE("hexdump: single byte produces one line", "[core][hexdump]") {
    const std::uint8_t data[1] = {0xAB};
    const auto result = hexdump(data, 1);
    // Should be exactly one line (terminated with '\n')
    REQUIRE(result.back() == '\n');
    const auto newline_count = std::count(result.begin(), result.end(), '\n');
    REQUIRE(newline_count == 1);
    REQUIRE(result.find("ab ") != std::string::npos);
}

TEST_CASE("hexdump: 16 bytes produces one line", "[core][hexdump]") {
    const std::uint8_t data[16] = {};
    const auto result = hexdump(data, 16);
    const auto newline_count = std::count(result.begin(), result.end(), '\n');
    REQUIRE(newline_count == 1);
}

TEST_CASE("hexdump: 17 bytes produces two lines", "[core][hexdump]") {
    const std::uint8_t data[17] = {};
    const auto result = hexdump(data, 17);
    const auto newline_count = std::count(result.begin(), result.end(), '\n');
    REQUIRE(newline_count == 2);
}

TEST_CASE("hexdump: 32 bytes produces two lines", "[core][hexdump]") {
    const std::uint8_t data[32] = {};
    const auto result = hexdump(data, 32);
    const auto newline_count = std::count(result.begin(), result.end(), '\n');
    REQUIRE(newline_count == 2);
}

TEST_CASE("hexdump: second line has correct offset", "[core][hexdump]") {
    const std::uint8_t data[32] = {};
    const auto result = hexdump(data, 32);
    // Find the second line (after the first '\n')
    const auto nl = result.find('\n');
    REQUIRE(nl != std::string::npos);
    const auto second_line = result.substr(nl + 1);
    // Second line starts at offset 16 = 0x0010
    REQUIRE(second_line.substr(0, 4) == "0010");
}

TEST_CASE("hexdump: span overload works", "[core][hexdump]") {
    const std::uint8_t raw[4] = {0x01, 0x02, 0x03, 0x04};
    const auto span = std::span<const std::byte>{
        reinterpret_cast<const std::byte*>(raw), 4
    };
    const auto result = hexdump(span);
    REQUIRE(result.find("01 ") != std::string::npos);
    REQUIRE(result.find("04 ") != std::string::npos);
}

TEST_CASE("hexdump: known byte sequence matches expected output", "[core][hexdump]") {
    // 4 bytes: 0x48='H', 0x65='e', 0x6C='l', 0x6C='l'
    const std::uint8_t data[4] = {0x48, 0x65, 0x6c, 0x6c};
    const auto result = hexdump(data, 4);
    REQUIRE(result.find("48 ") != std::string::npos);
    REQUIRE(result.find("65 ") != std::string::npos);
    REQUIRE(result.find("6c ") != std::string::npos);
    // ASCII column should show "Hell"
    REQUIRE(result.find("Hell") != std::string::npos);
}
