// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file config.hpp
/// Thin C++20 wrapper over toml++ (`toml::table`) that provides a
/// stable, exception-free API for reading TOML configuration files.
///
/// Design goals
/// ─────────────
/// • No exceptions leak out — all error paths return `std::optional`.
/// • Header-only: no .cpp needed; the wrapper is pure inline delegation
///   to the toml++ library that is already a dependency of `infra_config`.
/// • Composable: `section()` returns a child `Config` view so callers
///   can scope their reads without knowing the full key path.
///
/// Typical usage
/// ─────────────
/// @code
///   auto cfg = pvpgn::infra::config::Config::load_file("bnetd.toml");
///   if (!cfg) { /* handle error */ }
///   auto name = cfg->get_or<std::string>("server.name", "PvPGN");
///   if (auto net = cfg->section("network")) {
///       auto port = net->get_or<int>("port", 6112);
///   }
/// @endcode

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <toml++/toml.hpp>

namespace pvpgn::infra::config {

/// Validate that a byte string is well-formed UTF-8 (RFC 3629): rejects
/// overlong encodings, surrogate halves, and code points > U+10FFFF.
///
/// toml++ assumes its input is valid UTF-8 and reaches a `__builtin_unreachable`
/// (undefined behaviour) on certain malformed byte sequences. The TOML spec
/// requires UTF-8 input, so we reject invalid input up front rather than feed it
/// to the parser — this both matches the spec and hardens the parser entry
/// points against malformed config bytes (verified by the UBSan property tests).
[[nodiscard]] inline bool is_valid_utf8(std::string_view s) noexcept {
    const auto* p   = reinterpret_cast<const unsigned char*>(s.data());
    const auto* end = p + s.size();
    while (p < end) {
        const unsigned char c = *p;
        if (c < 0x80) { ++p; continue; }            // ASCII
        int len;
        if ((c & 0xE0) == 0xC0) {                    // 2-byte
            if (c < 0xC2) return false;              //   overlong
            len = 2;
        } else if ((c & 0xF0) == 0xE0) {             // 3-byte
            len = 3;
        } else if ((c & 0xF8) == 0xF0) {             // 4-byte
            if (c > 0xF4) return false;              //   > U+10FFFF
            len = 4;
        } else {
            return false;                            // 0x80-0xBF stray / 0xF5+
        }
        if (end - p < len) return false;             // truncated
        for (int i = 1; i < len; ++i) {
            if ((p[i] & 0xC0) != 0x80) return false; // bad continuation
        }
        if (len == 3) {
            if (c == 0xE0 && p[1] < 0xA0) return false;   // overlong
            if (c == 0xED && p[1] >= 0xA0) return false;  // UTF-16 surrogate
        } else if (len == 4) {
            if (c == 0xF0 && p[1] < 0x90) return false;   // overlong
            if (c == 0xF4 && p[1] >= 0x90) return false;  // > U+10FFFF
        }
        p += len;
    }
    return true;
}

/// Immutable view over a TOML table (or sub-table).
///
/// A `Config` wraps a `toml::table` by value. When constructed via
/// `section()` the child table is copied out of the parent, so the
/// child outlives the parent safely.
class Config {
public:
    // ── Construction ────────────────────────────────────────────────

    /// Parse a TOML string.
    /// @return populated Config, or `std::nullopt` on parse error.
    [[nodiscard]] static std::optional<Config>
    load_string(std::string_view toml_text) noexcept
    {
        // toml++ requires valid UTF-8 and has UB on some malformed sequences;
        // reject invalid input before parsing (treated as a parse error).
        if (!is_valid_utf8(toml_text)) {
            return std::nullopt;
        }
        try {
            return Config{toml::parse(toml_text)};
        } catch (...) {
            return std::nullopt;
        }
    }

    /// Parse a TOML file from disk.
    /// @return populated Config, or `std::nullopt` if the file cannot
    ///         be opened or contains a parse error.
    [[nodiscard]] static std::optional<Config>
    load_file(std::string_view path) noexcept
    {
        try {
            return Config{toml::parse_file(path)};
        } catch (...) {
            return std::nullopt;
        }
    }

    // ── Value access ─────────────────────────────────────────────────

    /// Return the value at `key` as type `T`, or `std::nullopt` if the
    /// key is absent or the value cannot be converted to `T`.
    ///
    /// Supported types: `bool`, `int64_t`, `double`, `std::string`,
    /// and any integer type implicitly convertible from `int64_t`.
    template <typename T>
    [[nodiscard]] std::optional<T> get(std::string_view key) const noexcept
    {
        try {
            if (auto v = table_[key].template value<T>())
                return *v;
        } catch (...) {}
        return std::nullopt;
    }

    /// Return the value at `key` as type `T`, or `fallback` if absent
    /// or unconvertible.
    template <typename T>
    [[nodiscard]] T get_or(std::string_view key, T fallback) const noexcept
    {
        return get<T>(key).value_or(std::move(fallback));
    }

    /// Return `true` if `key` exists in this table.
    [[nodiscard]] bool has(std::string_view key) const noexcept
    {
        return table_.contains(key);
    }

    /// Return the list of top-level keys in this table.
    [[nodiscard]] std::vector<std::string> keys() const
    {
        std::vector<std::string> result;
        result.reserve(table_.size());
        for (auto&& [k, _] : table_)
            result.emplace_back(static_cast<std::string_view>(k));
        return result;
    }

    // ── Sub-table access ─────────────────────────────────────────────

    /// Return a child `Config` for the sub-table named `section_name`,
    /// or `std::nullopt` if the key is absent or is not a table.
    [[nodiscard]] std::optional<Config>
    section(std::string_view section_name) const noexcept
    {
        try {
            if (auto* tbl = table_[section_name].as_table())
                return Config{*tbl};
        } catch (...) {}
        return std::nullopt;
    }

    // ── Raw access ───────────────────────────────────────────────────

    /// Direct access to the underlying `toml::table` for callers that
    /// need features not exposed by this wrapper.
    [[nodiscard]] const toml::table& raw() const noexcept { return table_; }

private:
    explicit Config(toml::table tbl) noexcept : table_(std::move(tbl)) {}

    toml::table table_;
};

}  // namespace pvpgn::infra::config
