// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/legacy_config/anongame_maplists_loader.hpp"

#include <array>
#include <cctype>
#include <fstream>
#include <string>
#include <string_view>

namespace pvpgn::infra::legacy_config {

namespace {

// 18 legacy queue type names, in legacy queue-index order.
inline constexpr std::array<std::string_view, kAnonGameQueueCount>
    kQueueNames{{
        "1v1", "2v2", "3v3", "4v4", "sffa", "at2v2", "tffa", "at3v3", "at4v4",
        "TY",
        "5v5", "6v6", "2v2v2", "3v3v3", "4v4v4", "2v2v2v2", "3v3v3v3",
        "at2v2v2",
    }};

int queue_index(std::string_view name) {
    for (std::size_t i = 0; i < kQueueNames.size(); ++i) {
        if (kQueueNames[i] == name) return static_cast<int>(i);
    }
    return -1;
}

std::string to_upper(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        out.push_back(static_cast<char>(
            std::toupper(static_cast<unsigned char>(c))));
    }
    return out;
}

// Strip `#` comment and trailing CR/LF + whitespace.
std::string_view strip_comment_and_trailing(std::string_view s) {
    const auto pos = s.find('#');
    if (pos != std::string_view::npos) s = s.substr(0, pos);
    std::size_t n = s.size();
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' ||
                     s[n - 1] == '\r' || s[n - 1] == '\n')) {
        --n;
    }
    return s.substr(0, n);
}

// Advance `i` past run of spaces/tabs. Returns true if any non-WS char
// remains.
bool skip_ws(std::string_view s, std::size_t& i) {
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
    return i < s.size();
}

// Read whitespace-delimited token starting at i; advance i past it.
// Returns empty view if no token.
std::string_view read_token(std::string_view s, std::size_t& i) {
    const std::size_t start = i;
    while (i < s.size() && s[i] != ' ' && s[i] != '\t') ++i;
    return s.substr(start, i - start);
}

// Read mapname starting at i. If `"`-quoted, reads up to next `"` or EOL;
// otherwise reads a whitespace-delimited token.
std::string_view read_mapname(std::string_view s, std::size_t& i) {
    if (i < s.size() && s[i] == '"') {
        ++i;  // skip opening quote
        const std::size_t start = i;
        while (i < s.size() && s[i] != '"') ++i;
        std::string_view out = s.substr(start, i - start);
        if (i < s.size() && s[i] == '"') ++i;  // skip closing quote
        return out;
    }
    return read_token(s, i);
}

}  // namespace

core::Result<MaplistsBundle>
load_anongame_maplists(std::string_view path) {
    std::ifstream in{std::string{path}};
    if (!in.is_open()) {
        return core::fail(core::make_error(
            core::StatusCode::NotFound,
            "anongame_maplists: cannot open '" + std::string{path} + "'"));
    }

    MaplistsBundle bundle;

    std::string raw_line;
    while (std::getline(in, raw_line)) {
        std::string_view line = strip_comment_and_trailing(raw_line);

        std::size_t i = 0;
        if (!skip_ws(line, i)) continue;

        // clienttag: must be exactly 4 chars
        const std::string_view tag = read_token(line, i);
        if (tag.size() != 4) continue;

        if (!skip_ws(line, i)) continue;
        const std::string_view qname = read_token(line, i);
        if (qname.empty()) continue;

        if (!skip_ws(line, i)) continue;
        const std::string_view mapname = read_mapname(line, i);
        if (mapname.empty()) continue;

        const int q = queue_index(qname);
        if (q < 0) continue;

        const std::string tag_key = to_upper(tag);
        auto& client = bundle.by_clienttag[tag_key];

        // Look up or append the mapname into the per-client map list.
        auto& names = client.map_payload.mapnames;
        std::size_t map_idx = names.size();
        for (std::size_t k = 0; k < names.size(); ++k) {
            if (names[k] == mapname) { map_idx = k; break; }
        }
        if (map_idx == names.size()) {
            // Need to add a new distinct map.
            if (names.size() >= kMaplistsMaxMaps) {
                // Cap reached; legacy silently drops further maps.
                continue;
            }
            names.emplace_back(mapname);
        }

        auto& queue = client.queue_map_indices[static_cast<std::size_t>(q)];
        if (queue.size() >= kMaplistsMaxMapsPerQueue) continue;

        // Skip exact duplicate (tag,queue,map) triples.
        bool already_in_queue = false;
        for (auto v : queue) {
            if (v == static_cast<std::uint8_t>(map_idx)) {
                already_in_queue = true;
                break;
            }
        }
        if (already_in_queue) continue;

        queue.push_back(static_cast<std::uint8_t>(map_idx));
    }

    return bundle;
}

}  // namespace pvpgn::infra::legacy_config
