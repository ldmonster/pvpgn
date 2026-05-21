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
