// SPDX-License-Identifier: GPL-2.0-or-later

#include "application/admin_commands/help_corpus.hpp"

#include <cctype>
#include <string_view>

namespace pvpgn::application::admin_commands {

namespace {

constexpr std::string_view trim_leading_slash(std::string_view s) noexcept {
    if (!s.empty() && s.front() == '/') s.remove_prefix(1);
    return s;
}

bool iequals(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        const auto ca = static_cast<unsigned char>(a[i]);
        const auto cb = static_cast<unsigned char>(b[i]);
        if (std::tolower(ca) != std::tolower(cb)) return false;
    }
    return true;
}

}  // namespace

const HelpEntry* HelpCorpus::find_by_alias(std::string_view name) const noexcept {
    const std::string_view needle = trim_leading_slash(name);
    for (const auto& entry : entries_) {
        for (const auto& alias : entry.aliases) {
            if (iequals(trim_leading_slash(alias), needle)) {
                return &entry;
            }
        }
    }
    return nullptr;
}

}  // namespace pvpgn::application::admin_commands
