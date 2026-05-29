// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file whisper_use_case.hpp
/// CHAT vertical (Batch 19a, partial). Pure decision for `/whisper`
/// (and `/w`, `/msg`, `/m`) commands.
///
/// Given the sender's name, the target name, the body text, and a
/// snapshot of the target's current state, the use-case decides
/// whether the whisper is `Delivered` or rejected (and why). The
/// caller (legacy bridge / future v3 chat router) is responsible for
/// snapshotting the inputs and performing the side effects suggested
/// by the verdict.
///
/// **Pure**: no I/O, no DI. The lookup of the target connection is
/// done by the caller before this function is invoked.

#include <string>
#include <string_view>

namespace pvpgn::application::chat {

struct WhisperTarget {
    bool online      = false;
    bool dnd         = false;  ///< Do-Not-Disturb / squelched globally.
    bool ignored_by  = false;  ///< target ignores the sender.
};

struct WhisperRequest {
    std::string_view sender;
    std::string_view target;
    std::string_view body;
    WhisperTarget    target_state;
};

enum class WhisperVerdict {
    Delivered,       ///< OK to forward to the target.
    EmptyBody,       ///< Body was whitespace-only; drop silently.
    NoTarget,        ///< Target name is empty (parser already filtered).
    SelfWhisper,     ///< Sender == target (case-insensitive).
    TargetOffline,
    TargetDnd,
    IgnoredBySender, ///< Sender ignores target (rare, but legacy checks it).
    IgnoredByTarget,
};

/// Pure decision. Both `sender` and `target` are compared
/// case-insensitively for the self-whisper rule.
WhisperVerdict decide_whisper(const WhisperRequest& req) noexcept;

}  // namespace pvpgn::application::chat
