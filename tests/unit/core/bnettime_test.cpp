// SPDX-License-Identifier: GPL-2.0-or-later
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "core/bnettime.hpp"

using namespace pvpgn::core;
using namespace std::chrono_literals;

// ---------------------------------------------------------------------------
// BnetTime::as_u64 / from_u64 round-trip
// ---------------------------------------------------------------------------

TEST_CASE("BnetTime::from_u64 / as_u64 round-trip", "[core][bnettime]") {
    const std::uint64_t v = 0x0000'0001'ABCD'EF12ULL;
    const auto bt = BnetTime::from_u64(v);
    REQUIRE(bt.upper == 0x0000'0001U);
    REQUIRE(bt.lower == 0xABCD'EF12U);
    REQUIRE(bt.as_u64() == v);
}

TEST_CASE("BnetTime zero value", "[core][bnettime]") {
    const auto bt = BnetTime{};
    REQUIRE(bt.upper == 0);
    REQUIRE(bt.lower == 0);
    REQUIRE(bt.as_u64() == 0);
}

TEST_CASE("BnetTime max value", "[core][bnettime]") {
    const std::uint64_t max = 0xFFFF'FFFF'FFFF'FFFFULL;
    const auto bt = BnetTime::from_u64(max);
    REQUIRE(bt.upper == 0xFFFF'FFFFU);
    REQUIRE(bt.lower == 0xFFFF'FFFFU);
    REQUIRE(bt.as_u64() == max);
}

// ---------------------------------------------------------------------------
// Epoch constant sanity check
// ---------------------------------------------------------------------------

TEST_CASE("kUnixEpochOffsetUnits matches known value", "[core][bnettime]") {
    // 11 644 473 600 seconds × 10 000 000 units/second
    constexpr std::uint64_t expected = 116'444'736'000'000'000ULL;
    REQUIRE(kUnixEpochOffsetUnits == expected);
}

// ---------------------------------------------------------------------------
// to_bnettime / from_bnettime round-trip
// ---------------------------------------------------------------------------

TEST_CASE("to_bnettime / from_bnettime round-trip at Unix epoch", "[core][bnettime]") {
    // Unix epoch = 1970-01-01 00:00:00 UTC
    const auto unix_epoch = std::chrono::system_clock::time_point{};
    const auto bt = to_bnettime(unix_epoch);

    // The FILETIME value for the Unix epoch is exactly kUnixEpochOffsetUnits.
    REQUIRE(bt.as_u64() == kUnixEpochOffsetUnits);

    // Round-trip back.
    const auto recovered = from_bnettime(bt);
    // Allow up to 1 µs of rounding error from the 100-ns unit conversion.
    const auto diff = std::chrono::abs(recovered - unix_epoch);
    REQUIRE(diff <= 1us);
}

TEST_CASE("to_bnettime / from_bnettime round-trip at known timestamp", "[core][bnettime]") {
    // 2000-01-01 00:00:00 UTC = Unix time 946 684 800
    const auto t2000 = std::chrono::system_clock::time_point{} +
                       std::chrono::seconds{946'684'800};
    const auto bt = to_bnettime(t2000);
    const auto recovered = from_bnettime(bt);
    const auto diff = std::chrono::abs(recovered - t2000);
    REQUIRE(diff <= 1us);
}

TEST_CASE("from_bnettime returns epoch for pre-Unix timestamps", "[core][bnettime]") {
    // A FILETIME value before the Unix epoch (e.g. year 1800).
    const auto bt = BnetTime{0, 1};  // very small value, before Unix epoch
    const auto tp = from_bnettime(bt);
    REQUIRE(tp == std::chrono::system_clock::time_point{});
}

// ---------------------------------------------------------------------------
// bnettime_to_string / bnettime_from_string
// ---------------------------------------------------------------------------

TEST_CASE("bnettime_to_string produces 'upper lower' format", "[core][bnettime]") {
    const BnetTime bt{29304451U, 3046165090U};
    REQUIRE(bnettime_to_string(bt) == "29304451 3046165090");
}

TEST_CASE("bnettime_from_string parses valid string", "[core][bnettime]") {
    const auto result = bnettime_from_string("29304451 3046165090");
    REQUIRE(result.has_value());
    REQUIRE(result->upper == 29304451U);
    REQUIRE(result->lower == 3046165090U);
}

TEST_CASE("bnettime_from_string round-trip", "[core][bnettime]") {
    const BnetTime original{27111902U, 3577643008U};  // legacy epoch example
    const auto str = bnettime_to_string(original);
    const auto parsed = bnettime_from_string(str);
    REQUIRE(parsed.has_value());
    REQUIRE(*parsed == original);
}

TEST_CASE("bnettime_from_string rejects missing space", "[core][bnettime]") {
    REQUIRE_FALSE(bnettime_from_string("293044513046165090").has_value());
}

TEST_CASE("bnettime_from_string rejects non-numeric input", "[core][bnettime]") {
    REQUIRE_FALSE(bnettime_from_string("abc 123").has_value());
    REQUIRE_FALSE(bnettime_from_string("123 xyz").has_value());
}

TEST_CASE("bnettime_from_string rejects empty string", "[core][bnettime]") {
    REQUIRE_FALSE(bnettime_from_string("").has_value());
}

TEST_CASE("bnettime_from_string rejects overflow", "[core][bnettime]") {
    // 5000000000 > 0xFFFFFFFF
    REQUIRE_FALSE(bnettime_from_string("5000000000 0").has_value());
    REQUIRE_FALSE(bnettime_from_string("0 5000000000").has_value());
}

// ---------------------------------------------------------------------------
// bnettime_add_tzbias
// ---------------------------------------------------------------------------

TEST_CASE("bnettime_add_tzbias zero bias is identity", "[core][bnettime]") {
    const BnetTime bt{29304451U, 3046165090U};
    REQUIRE(bnettime_add_tzbias(bt, 0) == bt);
}

TEST_CASE("bnettime_add_tzbias positive bias subtracts time", "[core][bnettime]") {
    // Start at Unix epoch in FILETIME units.
    const auto bt = BnetTime::from_u64(kUnixEpochOffsetUnits);
    // +60 minutes bias → subtract 3600 seconds = 36 000 000 000 units.
    const auto adjusted = bnettime_add_tzbias(bt, 60);
    const std::uint64_t expected = kUnixEpochOffsetUnits -
                                   60ULL * 60ULL * kBnetTimeUnitsPerSec;
    REQUIRE(adjusted.as_u64() == expected);
}

TEST_CASE("bnettime_add_tzbias negative bias adds time", "[core][bnettime]") {
    const auto bt = BnetTime::from_u64(kUnixEpochOffsetUnits);
    // -60 minutes bias → add 3600 seconds.
    const auto adjusted = bnettime_add_tzbias(bt, -60);
    const std::uint64_t expected = kUnixEpochOffsetUnits +
                                   60ULL * 60ULL * kBnetTimeUnitsPerSec;
    REQUIRE(adjusted.as_u64() == expected);
}

TEST_CASE("bnettime_add_tzbias clamps to zero on underflow", "[core][bnettime]") {
    // Very small timestamp + large positive bias → would underflow.
    const BnetTime bt{0, 1};
    const auto adjusted = bnettime_add_tzbias(bt, 60);
    REQUIRE(adjusted.as_u64() == 0);
}

// ---------------------------------------------------------------------------
// bnettime_now sanity check
// ---------------------------------------------------------------------------

TEST_CASE("bnettime_now returns a plausible value", "[core][bnettime]") {
    const auto bt = bnettime_now();
    // Must be after 2020-01-01 00:00:00 UTC in FILETIME units.
    // 2020-01-01 = Unix time 1577836800 → FILETIME = 1577836800*1e7 + offset
    const std::uint64_t min_2020 =
        kUnixEpochOffsetUnits + 1'577'836'800ULL * kBnetTimeUnitsPerSec;
    REQUIRE(bt.as_u64() > min_2020);
}
