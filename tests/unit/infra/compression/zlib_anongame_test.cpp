// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "infra/compression/zlib_anongame.hpp"

using namespace pvpgn::infra::compression;
using pvpgn::core::StatusCode;

namespace {

std::span<const std::uint8_t> as_span(const std::vector<std::uint8_t>& v) {
    return {v.data(), v.size()};
}

}  // namespace

TEST_CASE("zlib_anongame: round-trip empty",
          "[infra][compression][anongame]") {
    std::vector<std::uint8_t> raw;
    auto framed = anongame_compress(as_span(raw));
    REQUIRE(framed.has_value());
    REQUIRE(framed.value().size() >= 4);
    // header: raw_len=0
    REQUIRE(framed.value()[0] == 0);
    REQUIRE(framed.value()[1] == 0);
    auto back = anongame_decompress(as_span(framed.value()));
    REQUIRE(back.has_value());
    REQUIRE(back.value() == raw);
}

TEST_CASE("zlib_anongame: round-trip small ASCII payload",
          "[infra][compression][anongame]") {
    const std::string s = "http://server\0http://player\0http://tourney\0";
    std::vector<std::uint8_t> raw(s.begin(), s.end());
    auto framed = anongame_compress(as_span(raw));
    REQUIRE(framed.has_value());
    // raw_len matches header.
    const auto& f = framed.value();
    const std::uint16_t raw_len = std::uint16_t(f[0]) | (std::uint16_t(f[1]) << 8);
    REQUIRE(raw_len == raw.size());
    auto back = anongame_decompress(as_span(f));
    REQUIRE(back.has_value());
    REQUIRE(back.value() == raw);
}

TEST_CASE("zlib_anongame: round-trip 4 KiB compressible payload",
          "[infra][compression][anongame]") {
    std::vector<std::uint8_t> raw(4096, std::uint8_t{0xAB});
    auto framed = anongame_compress(as_span(raw));
    REQUIRE(framed.has_value());
    // Highly compressible -> framed must be much smaller than raw.
    REQUIRE(framed.value().size() < raw.size() / 2);
    auto back = anongame_decompress(as_span(framed.value()));
    REQUIRE(back.has_value());
    REQUIRE(back.value() == raw);
}

TEST_CASE("zlib_anongame: round-trip near-u16 boundary",
          "[infra][compression][anongame]") {
    std::vector<std::uint8_t> raw;
    raw.reserve(60000);
    for (std::size_t i = 0; i < 60000; ++i) {
        raw.push_back(static_cast<std::uint8_t>(i * 31u));
    }
    auto framed = anongame_compress(as_span(raw));
    REQUIRE(framed.has_value());
    auto back = anongame_decompress(as_span(framed.value()));
    REQUIRE(back.has_value());
    REQUIRE(back.value() == raw);
}

TEST_CASE("zlib_anongame: oversize raw payload rejected",
          "[infra][compression][anongame]") {
    std::vector<std::uint8_t> raw(70000, std::uint8_t{0x00});
    auto framed = anongame_compress(as_span(raw));
    REQUIRE_FALSE(framed.has_value());
    REQUIRE(framed.error().code() == StatusCode::OutOfRange);
}

TEST_CASE("zlib_anongame: decompress fails on short header",
          "[infra][compression][anongame]") {
    std::vector<std::uint8_t> too_short{0x01, 0x02};
    auto back = anongame_decompress(as_span(too_short));
    REQUIRE_FALSE(back.has_value());
    REQUIRE(back.error().code() == StatusCode::OutOfRange);
}

TEST_CASE("zlib_anongame: decompress fails when comp_len exceeds buffer",
          "[infra][compression][anongame]") {
    // raw_len=0, comp_len=10, but no payload follows.
    std::vector<std::uint8_t> bad{0x00, 0x00, 0x0A, 0x00};
    auto back = anongame_decompress(as_span(bad));
    REQUIRE_FALSE(back.has_value());
    REQUIRE(back.error().code() == StatusCode::OutOfRange);
}

TEST_CASE("zlib_anongame: decompress fails on corrupt stream",
          "[infra][compression][anongame]") {
    // header says raw_len=4, comp_len=4, then 4 bytes of junk that are
    // definitely not a valid zlib stream.
    std::vector<std::uint8_t> bad{0x04, 0x00, 0x04, 0x00,
                                  0xDE, 0xAD, 0xBE, 0xEF};
    auto back = anongame_decompress(as_span(bad));
    REQUIRE_FALSE(back.has_value());
}

TEST_CASE("zlib_anongame: decompress detects raw_len mismatch",
          "[infra][compression][anongame]") {
    std::vector<std::uint8_t> raw{1, 2, 3, 4, 5, 6, 7, 8};
    auto framed = anongame_compress(as_span(raw));
    REQUIRE(framed.has_value());
    auto& f = framed.value();
    // Bump the declared raw_len so inflate produces fewer bytes than
    // promised.
    f[0] = static_cast<std::uint8_t>(raw.size() + 1);
    auto back = anongame_decompress(as_span(f));
    REQUIRE_FALSE(back.has_value());
}
