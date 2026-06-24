// SPDX-License-Identifier: GPL-2.0-or-later

#include "application/admin_commands/help_corpus_parser.hpp"

#include <istream>
#include <optional>
#include <string>
#include <string_view>

namespace pvpgn::application::admin_commands {

namespace {

// Strip trailing CR / LF (line-ending normalisation for files written
// on Windows hosts).
void rstrip_eol(std::string& line) {
    while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) {
        line.pop_back();
    }
}

// Return offset of the first non-whitespace character, or line.size()
// if the line is all whitespace.
std::size_t leading_ws_end(std::string_view line) noexcept {
    std::size_t i = 0;
    while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
    return i;
}

// Expand tabs to three spaces, mirroring the legacy describe_command
// behaviour. Done at parse time so the model holds presentation-
// ready strings.
std::string expand_tabs(std::string_view in) {
    std::string out;
    out.reserve(in.size());
    for (const char c : in) {
        if (c == '\t') {
            out.append("   ", 3);
        } else {
            out.push_back(c);
        }
    }
    return out;
}

// Parse a `%`-header line into an entry's alias list.
//
// Input is the entire line (untrimmed). `head_off` points at the
// `%` character. Anything after an optional `#` (anywhere on the
// line) is dropped. Tokens are whitespace-separated; the leading `%`
// is stripped off the very first token. Each surviving token is
// prefixed with `/` to match router conventions.
void parse_header(std::string_view line, std::size_t head_off, HelpEntry& out) {
    // Slice off `#` comment (if any), starting from the % position.
    std::size_t end = line.size();
    for (std::size_t i = head_off; i < line.size(); ++i) {
        if (line[i] == '#') {
            end = i;
            break;
        }
    }
    const std::string_view view = line.substr(head_off, end - head_off);

    std::string token;
    bool first_token_seen = false;

    auto flush = [&]() {
        if (token.empty()) return;
        if (!first_token_seen) {
            // The very first token of a `%` line carries the `%`
            // prefix (e.g. "%help"). Strip it.
            if (token.front() == '%') token.erase(0, 1);
            first_token_seen = true;
        }
        if (!token.empty()) {
            std::string alias;
            alias.reserve(token.size() + 1);
            alias.push_back('/');
            alias.append(token);
            out.aliases.push_back(std::move(alias));
        }
        token.clear();
    };

    for (const char c : view) {
        if (c == ' ' || c == '\t') {
            flush();
        } else {
            token.push_back(c);
        }
    }
    flush();
}

}  // namespace

core::Result<HelpCorpus, core::Error>
parse_help_corpus(std::istream& in) {
    HelpCorpus corpus;
    std::optional<HelpEntry> current;
    std::string line;

    while (std::getline(in, line)) {
        rstrip_eol(line);
        const std::size_t first = leading_ws_end(line);

        if (first < line.size() && line[first] == '%') {
            // New entry header. Flush any in-progress entry.
            if (current) {
                corpus.add(std::move(*current));
                current.reset();
            }
            HelpEntry entry;
            parse_header(line, first, entry);
            // Drop entries with no aliases (malformed `%   ` lines).
            if (!entry.aliases.empty()) {
                current.emplace(std::move(entry));
            }
            continue;
        }

        // Pre-header lines and lines outside any entry are skipped.
        if (!current) continue;

        // Full-line `#` comment: drop.
        if (first < line.size() && line[first] == '#') continue;

        // Truncate at first `#` (trailing comment).
        std::string_view body{line};
        const std::size_t hash = body.find('#');
        if (hash != std::string_view::npos) body = body.substr(0, hash);

        std::string expanded = expand_tabs(body);
        // Drop lines that are empty after trimming -- legacy code
        // checked `line[i] != '\0'` after the leading-space skip; the
        // simpler check is "nothing but whitespace".
        const bool all_blank =
            expanded.find_first_not_of(" \t") == std::string::npos;
        if (all_blank) continue;

        current->description_lines.push_back(std::move(expanded));
    }

    if (current) {
        corpus.add(std::move(*current));
    }

    return corpus;
}

}  // namespace pvpgn::application::admin_commands
