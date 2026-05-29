// SPDX-License-Identifier: GPL-2.0-or-later

/// @file channel_config_loader.cpp
/// Implementation of ChannelConfigLoader.
///
/// Parses the legacy `channel.conf` columnar format:
///   "special name"  "short name"  cltag  bots  ops  log  ctry  realm  max  mod
///
/// Column indices (0-based):
///   0  special name  — quoted string or NONE
///   1  short name    — quoted string
///   2  cltag         — 4-char tag or NULL
///   3  bots          — true/false
///   4  ops           — true/false
///   5  log           — true/false
///   6  ctry          — quoted string or NULL
///   7  realm         — quoted string or NULL
///   8  max           — integer (-1 = unlimited)
///   9  mod           — true/false (moderated)
///
/// The loader extracts:
///   - name:        col[0] if not NONE, else col[1]
///   - max_members: col[8] (-1 → 0 = unlimited)
///   - permanent:   always true for entries in this file

#include "infra/config/channel_config_loader.hpp"

#include <fstream>
#include <sstream>
#include <string>

namespace pvpgn::infra::config {

namespace {

/// Extract the next token from @p line starting at @p pos.
/// A token is either:
///   - a double-quoted string  → returns the content without quotes
///   - an unquoted word        → returns the word as-is
/// Advances @p pos past the token and any trailing whitespace.
std::string next_token(const std::string& line, std::size_t& pos) {
    // Skip leading whitespace
    while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t')) {
        ++pos;
    }
    if (pos >= line.size()) return {};

    if (line[pos] == '"') {
        // Quoted string
        ++pos;  // skip opening quote
        std::string result;
        while (pos < line.size() && line[pos] != '"') {
            result += line[pos++];
        }
        if (pos < line.size()) ++pos;  // skip closing quote
        return result;
    }

    // Unquoted token
    std::size_t start = pos;
    while (pos < line.size() && line[pos] != ' ' && line[pos] != '\t') {
        ++pos;
    }
    return line.substr(start, pos - start);
}

/// Parse a single data line into a ChannelConfigEntry.
/// Returns false if the line has fewer than 9 columns.
bool parse_line(const std::string& line, ChannelConfigEntry& out) {
    std::size_t pos = 0;

    // col 0: special name (quoted or NONE)
    std::string special = next_token(line, pos);
    // col 1: short name (quoted)
    std::string shortname = next_token(line, pos);
    // col 2: cltag
    /*std::string cltag =*/ next_token(line, pos);
    // col 3: bots
    /*std::string bots =*/ next_token(line, pos);
    // col 4: ops
    /*std::string ops =*/ next_token(line, pos);
    // col 5: log
    /*std::string log =*/ next_token(line, pos);
    // col 6: ctry
    /*std::string ctry =*/ next_token(line, pos);
    // col 7: realm
    /*std::string realm =*/ next_token(line, pos);
    // col 8: max
    std::string max_str = next_token(line, pos);

    if (special.empty() || shortname.empty() || max_str.empty()) {
        return false;
    }

    // Determine display name
    out.name = (special == "NONE") ? shortname : special;

    // Parse max_members (-1 → 0 = unlimited)
    try {
        int max_val = std::stoi(max_str);
        out.max_members = (max_val < 0) ? 0u : static_cast<std::uint32_t>(max_val);
    } catch (...) {
        out.max_members = 0;
    }

    out.permanent = true;
    out.flags = 0x41;  // Permanent | AllowBots
    out.topic.clear();

    return true;
}

}  // namespace

// ---------------------------------------------------------------------------
// ChannelConfigLoader::load
// ---------------------------------------------------------------------------

std::vector<ChannelConfigEntry>
ChannelConfigLoader::load(const std::filesystem::path& config_path) {
    std::ifstream file(config_path);
    if (!file.is_open()) {
        return {};
    }

    std::vector<ChannelConfigEntry> entries;
    std::string line;

    while (std::getline(file, line)) {
        // Strip trailing CR (Windows line endings)
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        // Skip blank lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }

        // Skip lines that start with whitespace-only
        bool all_ws = true;
        for (char c : line) {
            if (c != ' ' && c != '\t') { all_ws = false; break; }
        }
        if (all_ws) continue;

        ChannelConfigEntry entry;
        if (parse_line(line, entry) && !entry.name.empty()) {
            entries.push_back(std::move(entry));
        }
    }

    return entries;
}

// ---------------------------------------------------------------------------
// ChannelConfigLoader::defaults
// ---------------------------------------------------------------------------

std::vector<ChannelConfigEntry> ChannelConfigLoader::defaults() {
    return {
        ChannelConfigEntry{"The Void",       "",  0x41, 0, true},
        ChannelConfigEntry{"Starcraft USA-1","",  0x41, 0, true},
        ChannelConfigEntry{"Diablo II",      "",  0x41, 0, true},
        ChannelConfigEntry{"Warcraft 3",     "",  0x41, 0, true},
        ChannelConfigEntry{"Chat",           "",  0x41, 0, true},
    };
}

}  // namespace pvpgn::infra::config
