// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file string_table.hpp
/// Minimal i18n port. The application layer asks for a
/// localised string by `key` + optional locale tag and receives the
/// raw template back (with positional placeholders `{0}`, `{1}`, ...
/// already substituted). Implementations own:
///   * locale fallback policy (e.g. `ruRU` -> `enUS` -> default),
///   * template parsing (printf-style is NOT supported; we settle on
///     the `{N}` form to keep the surface tiny and locale-neutral),
///   * loading / caching of the underlying table.
///
/// This port exists so the chat reply sink and any
/// reply path can produce user-visible text without
/// reaching into the legacy `localize()` machinery, which is
/// per-connection, depends on the legacy `t_account` globals, and
/// is unreachable from the application layer.

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace pvpgn::application::i18n {

/// Read-only string table. `format` is the one-and-only entry point;
/// implementations decide whether unknown keys return a sentinel, an
/// empty string, or the key itself.
class IStringTable {
public:
    virtual ~IStringTable() = default;

    /// Resolve `key` against `locale` (or the implementation's
    /// fallback locale if `locale` is empty / unknown), perform
    /// positional `{0}..{9}` substitution against `args`, and return
    /// the resulting string. Implementations MUST NOT throw.
    /// Args beyond `{9}` are ignored. Unmatched placeholders are
    /// emitted verbatim (`{3}` stays `{3}`) so misconfigured tables
    /// are diagnosable from log output.
    virtual std::string format(
        std::string_view key,
        std::string_view locale,
        std::span<const std::string_view> args) const noexcept = 0;

    /// Convenience: zero-arg lookup.
    std::string format(std::string_view key,
                       std::string_view locale) const noexcept {
        return format(key, locale, {});
    }
};

/// In-memory implementation backed by a flat
/// `(locale, key) -> template` map. Look-up order:
///   1. exact `(locale, key)`
///   2. `("", key)`        -- table-wide default
///   3. literal `key`      -- last-ditch sentinel (so the test logs
///                            show which key was missing)
class MapStringTable final : public IStringTable {
public:
    /// Insert / overwrite an entry. `locale` may be empty to mean
    /// "default fallback".
    void set(std::string_view locale, std::string_view key,
             std::string_view tmpl);

    using IStringTable::format;  // expose 2-arg convenience overload
    std::string format(
        std::string_view key,
        std::string_view locale,
        std::span<const std::string_view> args) const noexcept override;

private:
    struct Entry {
        std::string locale;   // empty == default
        std::string key;
        std::string tmpl;
    };
    // Linear scan; tables are small (dozens of entries) so the
    // big-O does not matter and the constant factor beats a hash
    // map for typical reply-table sizes.
    std::vector<Entry> entries_;
};

}  // namespace pvpgn::application::i18n
