// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/i18n/string_table.hpp"

#include <cctype>
#include <string>

namespace pvpgn::application::i18n {

namespace {

bool ieq(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        auto ca = static_cast<unsigned char>(a[i]);
        auto cb = static_cast<unsigned char>(b[i]);
        if (std::tolower(ca) != std::tolower(cb)) return false;
    }
    return true;
}

// Format `tmpl` by substituting `{0}`..`{9}` with `args[N]`.
// Unmatched placeholders are emitted verbatim.
std::string apply_args(std::string_view tmpl,
                       std::span<const std::string_view> args) {
    std::string out;
    out.reserve(tmpl.size());
    for (std::size_t i = 0; i < tmpl.size();) {
        if (tmpl[i] == '{' && i + 2 < tmpl.size() && tmpl[i + 2] == '}') {
            char c = tmpl[i + 1];
            if (c >= '0' && c <= '9') {
                std::size_t idx = static_cast<std::size_t>(c - '0');
                if (idx < args.size()) {
                    out.append(args[idx]);
                    i += 3;
                    continue;
                }
            }
        }
        out.push_back(tmpl[i]);
        ++i;
    }
    return out;
}

}  // namespace

void MapStringTable::set(std::string_view locale, std::string_view key,
                         std::string_view tmpl) {
    for (auto& e : entries_) {
        if (ieq(e.locale, locale) && e.key == key) {
            e.tmpl = std::string{tmpl};
            return;
        }
    }
    entries_.push_back(Entry{
        std::string{locale}, std::string{key}, std::string{tmpl}});
}

std::string MapStringTable::format(
    std::string_view key,
    std::string_view locale,
    std::span<const std::string_view> args) const noexcept {
    // Locale-specific match first.
    if (!locale.empty()) {
        for (const auto& e : entries_) {
            if (e.key == key && ieq(e.locale, locale)) {
                return apply_args(e.tmpl, args);
            }
        }
        // Language-only fallback: strip the country part if the
        // tag looks like `xxYY` (4 letters). `ruRU` -> `ru`,
        // `deDE` -> `de`. Locale tags are checked case-insensitively
        // via `ieq` above, so we just shorten and re-scan.
        if (locale.size() >= 3) {
            const std::string_view lang = locale.substr(0, 2);
            for (const auto& e : entries_) {
                if (e.key == key && ieq(e.locale, lang)) {
                    return apply_args(e.tmpl, args);
                }
            }
        }
    }
    // Default-locale fallback.
    for (const auto& e : entries_) {
        if (e.key == key && e.locale.empty()) {
            return apply_args(e.tmpl, args);
        }
    }
    // Sentinel: emit the key itself so missing translations are
    // visible in logs / packet captures.
    return std::string{key};
}

}  // namespace pvpgn::application::i18n
