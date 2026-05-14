// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/chat/chat_command.hpp"

#include <cctype>

namespace pvpgn::application::chat {

namespace {

bool is_ws(char c) {
    auto u = static_cast<unsigned char>(c);
    return u == ' ' || u == '\t' || u == '\r' || u == '\n';
}

std::string_view trim(std::string_view s) {
    std::size_t b = 0;
    while (b < s.size() && is_ws(s[b])) ++b;
    std::size_t e = s.size();
    while (e > b && is_ws(s[e - 1])) --e;
    return s.substr(b, e - b);
}

bool ieq(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        auto ca = std::tolower(static_cast<unsigned char>(a[i]));
        auto cb = std::tolower(static_cast<unsigned char>(b[i]));
        if (ca != cb) return false;
    }
    return true;
}

bool is_whisper_prefix(std::string_view tok) {
    return ieq(tok, "/w") || ieq(tok, "/whisper")
        || ieq(tok, "/msg") || ieq(tok, "/m");
}

}  // namespace

ChatAction classify_chat_command(std::string_view raw) {
    auto s = trim(raw);
    if (s.empty()) return EmptyAction{};

    if (s.front() != '/') {
        return ChannelMessageAction{std::string{s}};
    }

    // First whitespace splits the leading token from the remainder.
    std::size_t sp = 0;
    while (sp < s.size() && !is_ws(s[sp])) ++sp;
    auto head = s.substr(0, sp);
    auto rest = sp < s.size() ? trim(s.substr(sp + 1)) : std::string_view{};

    if (is_whisper_prefix(head)) {
        // Need a target and a non-empty body. Without a target we
        // demote to an empty `/w` slash-command so the caller can
        // surface a usage hint.
        if (rest.empty()) {
            CommandAction cmd;
            cmd.name = std::string{head.substr(1)};
            for (auto& c : cmd.name) c = static_cast<char>(
                std::tolower(static_cast<unsigned char>(c)));
            return cmd;
        }
        std::size_t tsp = 0;
        while (tsp < rest.size() && !is_ws(rest[tsp])) ++tsp;
        auto target = rest.substr(0, tsp);
        auto body   = tsp < rest.size() ? trim(rest.substr(tsp + 1))
                                        : std::string_view{};
        WhisperAction w;
        w.target = std::string{target};
        w.body   = std::string{body};
        return w;
    }

    CommandAction cmd;
    cmd.name = std::string{head.substr(1)};
    for (auto& c : cmd.name) c = static_cast<char>(
        std::tolower(static_cast<unsigned char>(c)));
    cmd.args = std::string{rest};
    return cmd;
}

}  // namespace pvpgn::application::chat
