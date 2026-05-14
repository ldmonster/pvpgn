// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/legacy_config/anongame_infos_loader.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace pvpgn::infra::legacy_config {

namespace ply = pvpgn::application::anongame_infoply;
namespace pb  = pvpgn::protocol::bnet;

namespace {

std::string_view ltrim(std::string_view s) {
    std::size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
    return s.substr(i);
}

std::string_view rtrim(std::string_view s) {
    std::size_t n = s.size();
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' ||
                     s[n - 1] == '\r' || s[n - 1] == '\n')) {
        --n;
    }
    return s.substr(0, n);
}

std::string_view trim(std::string_view s) { return rtrim(ltrim(s)); }

std::string_view strip_legacy_comment(std::string_view s) {
    auto pos = s.rfind('#');
    if (pos == std::string_view::npos) return s;
    return rtrim(s.substr(0, pos));
}

bool is_section_header(std::string_view s, std::string& name_out) {
    if (s.size() < 2 || s.front() != '[' || s.back() != ']') return false;
    name_out.assign(s.begin() + 1, s.end() - 1);
    return true;
}

bool parse_kv(std::string_view line, std::string& key, std::string& value) {
    const auto eq = line.find('=');
    if (eq == std::string_view::npos) return false;
    key.assign(trim(line.substr(0, eq)));
    auto rhs = trim(line.substr(eq + 1));
    if (rhs.size() < 2 || rhs.front() != '"') return false;
    auto end_quote = rhs.find('"', 1);
    if (end_quote == std::string_view::npos) return false;
    value.assign(rhs.substr(1, end_quote - 1));
    return true;
}

struct GametypeRow {
    std::string_view name;
    std::uint8_t     id;
};
inline constexpr std::array<GametypeRow, 13> kGametypes{{
    {"1v1",       0},   {"2v2",       1},
    {"3v3",       2},   {"4v4",       3},
    {"sffa",      4},   {"tffa",      6},
    {"5v5",      10},   {"6v6",      11},
    {"2v2v2",    12},   {"3v3v3",    13},
    {"4v4v4",    14},   {"2v2v2v2",  15},
    {"3v3v3v3",  16},
}};

struct LadrRow {
    std::string_view ladder_id;
    char             tag_bytes[4];
};
inline constexpr std::array<LadrRow, 10> kLadrRows{{
    {"PG_1v1",   {'O', 'L', 'O', 'S'}},
    {"PG_team",  {'M', 'A', 'E', 'T'}},
    {"PG_ffa",   {' ', 'A', 'F', 'F'}},
    {"AT_2v2",   {'2', 'S', 'V', '2'}},
    {"AT_3v3",   {'3', 'S', 'V', '3'}},
    {"AT_4v4",   {'4', 'S', 'V', '4'}},
    {"clan_1v1", {'S', 'N', 'L', 'C'}},
    {"clan_2v2", {'2', 'N', 'L', 'C'}},
    {"clan_3v3", {'3', 'N', 'L', 'C'}},
    {"clan_4v4", {'4', 'N', 'L', 'C'}},
}};

std::uint32_t tag_to_u32(const char (&b)[4]) {
    return static_cast<std::uint32_t>(static_cast<std::uint8_t>(b[0])) |
           (static_cast<std::uint32_t>(static_cast<std::uint8_t>(b[1])) << 8) |
           (static_cast<std::uint32_t>(static_cast<std::uint8_t>(b[2])) << 16) |
           (static_cast<std::uint32_t>(static_cast<std::uint8_t>(b[3])) << 24);
}

bool split_pfx_sfx(std::string_view key, std::string_view prefix,
                   std::string_view suffix, std::string& base_out) {
    if (key.substr(0, prefix.size()) != prefix) return false;
    if (key.size() < prefix.size() + suffix.size() + 1) return false;
    if (key.substr(key.size() - suffix.size()) != suffix) return false;
    base_out.assign(key.substr(prefix.size(),
                               key.size() - prefix.size() - suffix.size()));
    return !base_out.empty();
}

// Per-locale DESC string buffers. Key "" = default block.
struct DescBucket {
    std::unordered_map<std::string, std::string> shorts;
    std::unordered_map<std::string, std::string> longs;
    std::unordered_map<std::string, std::string> ladder;
};

const std::string* lookup_with_fallback(
    const std::unordered_map<std::string, std::string>& primary,
    const std::unordered_map<std::string, std::string>& fallback,
    const std::string& key) {
    if (auto it = primary.find(key); it != primary.end()) return &it->second;
    if (auto it = fallback.find(key); it != fallback.end()) return &it->second;
    return nullptr;
}

ply::AnonGameInfoSnapshot build_snapshot(
    const std::optional<std::string>& server_url,
    const std::optional<std::string>& player_url,
    const std::optional<std::string>& tourney_url,
    const std::optional<std::string>& clan_url,
    const std::unordered_map<std::string, std::string>& ladr_url,
    const DescBucket&                                   self,
    const DescBucket&                                   fallback,
    bool has_any_ladder_key) {
    ply::AnonGameInfoSnapshot snap{};

    if (server_url && player_url && tourney_url) {
        pb::AnonGameUrlPayload url{};
        url.urls.push_back(*server_url);
        url.urls.push_back(*player_url);
        url.urls.push_back(*tourney_url);
        if (clan_url) url.urls.push_back(*clan_url);
        snap.url = std::move(url);
    }

    pb::AnonGameDescPayload desc{};
    for (const auto& row : kGametypes) {
        const std::string name{row.name};
        const std::string* s = lookup_with_fallback(self.shorts,
                                                    fallback.shorts, name);
        const std::string* l = lookup_with_fallback(self.longs,
                                                    fallback.longs, name);
        if (!s || !l) continue;
        pb::AnonGameDescEntry e{};
        e.section_id  = 0;
        e.gametype_id = row.id;
        e.short_desc  = *s;
        e.long_desc   = *l;
        desc.entries.push_back(std::move(e));
    }
    if (!desc.entries.empty()) snap.desc = std::move(desc);

    if (has_any_ladder_key) {
        pb::AnonGameLadrPayload ladr{};
        for (const auto& row : kLadrRows) {
            pb::AnonGameLadrEntry e{};
            e.tag = tag_to_u32(row.tag_bytes);
            const std::string id{row.ladder_id};
            if (const std::string* d = lookup_with_fallback(self.ladder,
                                                            fallback.ladder,
                                                            id)) {
                e.desc = *d;
            }
            if (auto it = ladr_url.find(id); it != ladr_url.end()) {
                e.url = it->second;
            }
            ladr.entries.push_back(std::move(e));
        }
        snap.ladr = std::move(ladr);
    }

    return snap;
}

bool is_reserved_section(std::string_view s) {
    if (s == "URL" || s == "DEFAULT_DESC" || s == "THUMBS_DOWN_LIMIT")
        return true;
    constexpr std::string_view kIconPrefix = "ICON_REQUIRED_";
    return s.substr(0, kIconPrefix.size()) == kIconPrefix;
}

}  // namespace

core::Result<MultilocaleSnapshotSet> load_anongame_infos_multilocale(
    std::string_view path) {
    std::ifstream in{std::string{path}};
    if (!in.is_open()) {
        return core::fail(core::make_error(
            core::StatusCode::NotFound,
            std::string{"load_anongame_infos_multilocale: cannot open "} +
                std::string{path}));
    }

    std::optional<std::string> server_url;
    std::optional<std::string> player_url;
    std::optional<std::string> tourney_url;
    std::optional<std::string> clan_url;

    std::unordered_map<std::string, std::string> ladr_url;
    bool has_any_ladder_key = false;

    // Per-locale DESC buckets. Empty key "" = `[DEFAULT_DESC]`. Other
    // keys are the langID from `[<langID>]` section headers.
    std::unordered_map<std::string, DescBucket> buckets;
    buckets[""];  // ensure default bucket exists

    std::string section;
    std::string raw_line;
    std::string key;
    std::string value;
    while (std::getline(in, raw_line)) {
        std::string_view line = raw_line;
        line = trim(strip_legacy_comment(line));
        if (line.empty()) continue;

        std::string maybe_section;
        if (is_section_header(line, maybe_section)) {
            section = std::move(maybe_section);
            if (!is_reserved_section(section)) {
                // Materialise the bucket for any new locale section.
                buckets[section];
            }
            continue;
        }

        if (section == "URL") {
            if (!parse_kv(line, key, value)) continue;
            if (key == "server_URL")        server_url  = value;
            else if (key == "player_URL")   player_url  = value;
            else if (key == "tourney_URL")  tourney_url = value;
            else if (key == "clan_URL")     clan_url    = value;
            else {
                std::string ladder_id;
                if (split_pfx_sfx(key, "ladder_", "_URL", ladder_id)) {
                    ladr_url[ladder_id] = value;
                    has_any_ladder_key  = true;
                }
            }
            continue;
        }

        // DEFAULT_DESC and per-locale sections share the same key
        // grammar; route to the appropriate bucket.
        std::string langID;
        if (section == "DEFAULT_DESC") {
            langID = "";
        } else if (!section.empty() && !is_reserved_section(section)) {
            langID = section;
        } else {
            continue;  // unknown / non-DESC reserved section
        }

        if (!parse_kv(line, key, value)) continue;
        auto& bucket = buckets[langID];
        std::string base;
        if (split_pfx_sfx(key, "gametype_", "_short", base)) {
            bucket.shorts[base] = value;
        } else if (split_pfx_sfx(key, "gametype_", "_long", base)) {
            bucket.longs[base] = value;
        } else if (split_pfx_sfx(key, "ladder_", "_desc", base)) {
            bucket.ladder[base] = value;
            has_any_ladder_key  = true;
        }
    }

    const DescBucket& default_bucket = buckets[""];

    MultilocaleSnapshotSet out;
    static const DescBucket kEmptyBucket{};
    // Default snapshot: no fallback (it IS the fallback for others).
    out.default_snapshot = build_snapshot(server_url, player_url, tourney_url,
                                          clan_url, ladr_url, default_bucket,
                                          kEmptyBucket, has_any_ladder_key);
    for (const auto& [lang, bucket] : buckets) {
        if (lang.empty()) continue;
        out.by_lang[lang] =
            build_snapshot(server_url, player_url, tourney_url, clan_url,
                           ladr_url, bucket, default_bucket,
                           has_any_ladder_key);
    }
    return out;
}

core::Result<ply::AnonGameInfoSnapshot> load_anongame_infos(
    std::string_view path) {
    auto r = load_anongame_infos_multilocale(path);
    if (!r) return core::fail(r.error());
    return std::move(r.value().default_snapshot);
}

}  // namespace pvpgn::infra::legacy_config
