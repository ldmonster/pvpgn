// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/chat/whisper_use_case.hpp"

#include <cctype>

namespace pvpgn::application::chat {

namespace {

bool is_ws(char c) {
    auto u = static_cast<unsigned char>(c);
    return u == ' ' || u == '\t' || u == '\r' || u == '\n';
}

bool is_blank(std::string_view s) {
    for (char c : s) if (!is_ws(c)) return false;
    return true;
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

}  // namespace

WhisperVerdict decide_whisper(const WhisperRequest& req) noexcept {
    if (req.target.empty())                    return WhisperVerdict::NoTarget;
    if (is_blank(req.body))                    return WhisperVerdict::EmptyBody;
    if (ieq(req.sender, req.target))           return WhisperVerdict::SelfWhisper;
    if (!req.target_state.online)              return WhisperVerdict::TargetOffline;
    if (req.target_state.dnd)                  return WhisperVerdict::TargetDnd;
    if (req.target_state.ignored_by)           return WhisperVerdict::IgnoredByTarget;
    return WhisperVerdict::Delivered;
}

}  // namespace pvpgn::application::chat
