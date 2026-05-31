// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/legacy_config/icon_req_loader.hpp"

#include <cctype>
#include <charconv>
#include <fstream>
#include <string>
#include <string_view>

namespace pvpgn::infra::legacy_config {

namespace {

std::string_view trim(std::string_view s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
        s.remove_prefix(1);
    }
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
        s.remove_suffix(1);
    }
    return s;
}

// Parse "LevelN" -> N (1-based). Returns 0 on failure.
int parse_level_key(std::string_view key) {
    constexpr std::string_view kPrefix = "Level";
    if (key.size() <= kPrefix.size()) return 0;
    if (key.substr(0, kPrefix.size()) != kPrefix) return 0;
    int n = 0;
    auto suffix = key.substr(kPrefix.size());
    auto [p, ec] = std::from_chars(
        suffix.data(), suffix.data() + suffix.size(), n);
    if (ec != std::errc{} || p != suffix.data() + suffix.size()) return 0;
    return n;
}

bool is_section(std::string_view line, std::string_view& name_out) {
    if (line.size() < 2 || line.front() != '[' || line.back() != ']') {
        return false;
    }
    name_out = line.substr(1, line.size() - 2);
    return true;
}

}  // namespace

core::Result<IconReqTable> load_icon_req_table(std::string_view path) {
    std::ifstream in{std::string{path}};
    if (!in) {
        return core::fail(core::make_error(
            core::StatusCode::NotFound,
            "icon_req_loader: cannot open " + std::string{path}));
    }

    IconReqTable out{};
    enum class Section { None, War3, W3xp, Tourney };
    Section sec = Section::None;

    std::string raw;
    while (std::getline(in, raw)) {
        // Strip CR if present (Windows line endings).
        if (!raw.empty() && raw.back() == '\r') raw.pop_back();
        std::string_view line = trim(raw);
        if (line.empty() || line.front() == '#') continue;

        std::string_view sect;
        if (is_section(line, sect)) {
            if (sect == "ICON_REQUIRED_RACE_WINS_WAR3")  sec = Section::War3;
            else if (sect == "ICON_REQUIRED_RACE_WINS_W3XP") sec = Section::W3xp;
            else if (sect == "ICON_REQUIRED_TOURNEY_WINS")   sec = Section::Tourney;
            else                                              sec = Section::None;
            continue;
        }
        if (sec == Section::None) continue;

        // key = value
        auto eq = line.find('=');
        if (eq == std::string_view::npos) continue;
        auto key = trim(line.substr(0, eq));
        auto val = trim(line.substr(eq + 1));

        int level = parse_level_key(key);
        if (level <= 0) continue;

        // Strip trailing inline comment if any.
        if (auto h = val.find('#'); h != std::string_view::npos) {
            val = trim(val.substr(0, h));
        }

        unsigned int n = 0;
        auto [p, ec] = std::from_chars(
            val.data(), val.data() + val.size(), n);
        if (ec != std::errc{}) {
            return core::fail(core::make_error(
                core::StatusCode::InvalidArgument,
                "icon_req_loader: malformed value for "
                + std::string{key}));
        }
        if (n > 0xFFFFu) n = 0xFFFFu;
        const auto v = static_cast<std::uint16_t>(n);

        switch (sec) {
            case Section::War3:
                if (level >= 1 && level <= static_cast<int>(kIconReqWar3Levels)) {
                    out.war3[static_cast<std::size_t>(level - 1)] = v;
                }
                break;
            case Section::W3xp:
                if (level >= 1 && level <= static_cast<int>(kIconReqW3xpLevels)) {
                    out.w3xp[static_cast<std::size_t>(level - 1)] = v;
                }
                break;
            case Section::Tourney:
                if (level >= 1 && level <= static_cast<int>(kIconReqTourneyLevels)) {
                    out.tourney[static_cast<std::size_t>(level - 1)] = v;
                }
                break;
            case Section::None:
                break;
        }
    }

    return out;
}

}  // namespace pvpgn::infra::legacy_config
