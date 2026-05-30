// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file bnettime.hpp
/// Battle.net / Windows FILETIME epoch conversion utilities.
///
/// Battle.net timestamps are 64-bit Windows FILETIME values: the number of
/// 100-nanosecond intervals since 1601-01-01 00:00:00 UTC.  They are
/// transmitted on the wire as two 32-bit unsigned integers (upper, lower)
/// stored in little-endian byte order.
///
/// This header is a pure C++20 replacement for `src/common/bnettime.cpp/.h`.
/// It has zero dependencies on legacy headers and no global state.
///
/// Key constants (derived from the legacy implementation comments):
///   - 1 unit  = 100 ns  = 1e-7 s
///   - 2^32 units ≈ 429.497 s  (the "upper" counter period)
///   - Unix epoch offset from FILETIME epoch: 11 644 473 600 seconds
///     (i.e. 1970-01-01 is 116 444 736 000 000 000 FILETIME units after
///      1601-01-01)

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace pvpgn::core {

/// A 64-bit Windows FILETIME value split into two 32-bit halves.
/// `upper` is the high 32 bits; `lower` is the low 32 bits.
/// On the wire both halves are transmitted as little-endian uint32.
struct BnetTime {
    std::uint32_t upper{};  ///< High 32 bits (units of ~429.5 s)
    std::uint32_t lower{};  ///< Low  32 bits (units of 100 ns)

    /// Combine into a single 64-bit FILETIME value.
    [[nodiscard]] constexpr std::uint64_t as_u64() const noexcept {
        return (static_cast<std::uint64_t>(upper) << 32) |
               static_cast<std::uint64_t>(lower);
    }

    /// Construct from a raw 64-bit FILETIME value.
    [[nodiscard]] static constexpr BnetTime from_u64(std::uint64_t v) noexcept {
        return BnetTime{
            static_cast<std::uint32_t>(v >> 32),
            static_cast<std::uint32_t>(v & 0xFFFF'FFFF)
        };
    }

    constexpr bool operator==(const BnetTime&) const noexcept = default;
};

// ---------------------------------------------------------------------------
// Epoch constants
// ---------------------------------------------------------------------------

/// Number of 100-ns intervals per second.
inline constexpr std::uint64_t kBnetTimeUnitsPerSec = 10'000'000ULL;

/// Offset of the Unix epoch (1970-01-01) from the FILETIME epoch (1601-01-01)
/// expressed in 100-ns intervals.
/// = 11 644 473 600 seconds × 10 000 000 units/second
inline constexpr std::uint64_t kUnixEpochOffsetUnits = 116'444'736'000'000'000ULL;

// ---------------------------------------------------------------------------
// Conversion helpers
// ---------------------------------------------------------------------------

/// Convert a `std::chrono::system_clock::time_point` to a `BnetTime`.
/// Sub-second precision is preserved to microsecond granularity.
[[nodiscard]] inline BnetTime
to_bnettime(std::chrono::system_clock::time_point tp) noexcept {
    using namespace std::chrono;
    const auto since_epoch = tp.time_since_epoch();
    const auto secs  = duration_cast<seconds>(since_epoch).count();
    const auto usecs = duration_cast<microseconds>(
                           since_epoch - seconds(secs)).count();

    // Convert to 100-ns units and add the FILETIME epoch offset.
    const std::uint64_t units =
        static_cast<std::uint64_t>(secs) * kBnetTimeUnitsPerSec +
        static_cast<std::uint64_t>(usecs) * 10ULL +   // 1 µs = 10 × 100 ns
        kUnixEpochOffsetUnits;

    return BnetTime::from_u64(units);
}

/// Convert a `BnetTime` to a `std::chrono::system_clock::time_point`.
/// Returns the epoch time_point if the value pre-dates the Unix epoch.
[[nodiscard]] inline std::chrono::system_clock::time_point
from_bnettime(BnetTime bt) noexcept {
    using namespace std::chrono;
    const std::uint64_t units = bt.as_u64();
    if (units < kUnixEpochOffsetUnits) {
        return system_clock::time_point{};
    }
    const std::uint64_t unix_units = units - kUnixEpochOffsetUnits;
    // Convert 100-ns units to the system_clock duration (usually nanoseconds
    // or microseconds depending on the platform).
    const auto dur = duration_cast<system_clock::duration>(
        std::chrono::duration<std::uint64_t, std::ratio<1, 10'000'000>>{unix_units});
    return system_clock::time_point{dur};
}

/// Return the current time as a `BnetTime`.
[[nodiscard]] inline BnetTime bnettime_now() noexcept {
    return to_bnettime(std::chrono::system_clock::now());
}

// ---------------------------------------------------------------------------
// String serialisation (legacy wire format: "upper lower")
// ---------------------------------------------------------------------------

/// Serialise a `BnetTime` to the legacy string format used in account files:
/// two decimal unsigned integers separated by a space, e.g. "29304451 3046165090".
[[nodiscard]] inline std::string bnettime_to_string(BnetTime bt) {
    return std::to_string(bt.upper) + ' ' + std::to_string(bt.lower);
}

/// Parse a `BnetTime` from the legacy string format "upper lower".
/// Returns `std::nullopt` if the string is malformed.
[[nodiscard]] inline std::optional<BnetTime>
bnettime_from_string(std::string_view s) noexcept {
    // Find the space separator.
    const auto sp = s.find(' ');
    if (sp == std::string_view::npos) return std::nullopt;

    const auto upper_sv = s.substr(0, sp);
    const auto lower_sv = s.substr(sp + 1);
    if (upper_sv.empty() || lower_sv.empty()) return std::nullopt;

    // Parse each half as an unsigned decimal integer.
    auto parse_u32 = [](std::string_view sv) -> std::optional<std::uint32_t> {
        std::uint64_t acc = 0;
        for (char c : sv) {
            if (c < '0' || c > '9') return std::nullopt;
            acc = acc * 10 + static_cast<unsigned>(c - '0');
            if (acc > 0xFFFF'FFFFUL) return std::nullopt;
        }
        return static_cast<std::uint32_t>(acc);
    };

    const auto u = parse_u32(upper_sv);
    const auto l = parse_u32(lower_sv);
    if (!u || !l) return std::nullopt;

    return BnetTime{*u, *l};
}

// ---------------------------------------------------------------------------
// Timezone bias helper
// ---------------------------------------------------------------------------

/// Return the local timezone bias in minutes (positive = west of UTC,
/// negative = east of UTC), matching the legacy `local_tzbias()` semantics.
/// Uses `std::chrono` where possible; falls back to `std::mktime` for the
/// UTC→local conversion since C++20 does not yet expose a portable
/// `utc_clock` on all platforms.
[[nodiscard]] inline int local_tzbias_minutes() noexcept {
    const std::time_t now = std::time(nullptr);

    struct std::tm gmt_tm{};
    struct std::tm loc_tm{};

#if defined(_WIN32)
    if (gmtime_s(&gmt_tm, &now) != 0) return 0;
    if (localtime_s(&loc_tm, &now) != 0) return 0;
#else
    if (!gmtime_r(&now, &gmt_tm)) return 0;
    if (!localtime_r(&now, &loc_tm)) return 0;
#endif

    const std::time_t gmt_as_local = std::mktime(&gmt_tm);
    const std::time_t loc_as_local = std::mktime(&loc_tm);
    if (gmt_as_local == static_cast<std::time_t>(-1)) return 0;
    if (loc_as_local == static_cast<std::time_t>(-1)) return 0;

    // Positive result means local is ahead of UTC (east); legacy returns
    // negative for east, positive for west — we match that convention.
    // Cast via long long to avoid MSVC C4244 (time_t → int narrowing).
    if (loc_as_local > gmt_as_local)
        return -static_cast<int>(static_cast<long long>(loc_as_local - gmt_as_local) / 60LL);
    return static_cast<int>(static_cast<long long>(gmt_as_local - loc_as_local) / 60LL);
}

/// Adjust a `BnetTime` by a timezone bias (in minutes).
/// Subtracts `bias_minutes * 60` seconds from the timestamp, matching the
/// legacy `bnettime_add_tzbias()` behaviour.
[[nodiscard]] inline BnetTime
bnettime_add_tzbias(BnetTime bt, int bias_minutes) noexcept {
    const std::int64_t delta_units =
        static_cast<std::int64_t>(bias_minutes) * 60LL *
        static_cast<std::int64_t>(kBnetTimeUnitsPerSec);
    const std::int64_t raw = static_cast<std::int64_t>(bt.as_u64()) - delta_units;
    if (raw < 0) return BnetTime{};
    return BnetTime::from_u64(static_cast<std::uint64_t>(raw));
}

}  // namespace pvpgn::core
