// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file chat_reply_sink.hpp
/// `IChatReplySink` -- application-layer port the chat bridge calls
/// when a `decide_whisper` verdict requires sending a user-visible
/// reply back to the sender. This is the minimum seam needed for the
/// bridge to *own* outbound text for the rejection arms
/// (TargetOffline / TargetDnd / SelfWhisper / IgnoredByTarget /
/// IgnoredBySender / NoTarget) without depending directly on the
/// legacy `message_send_text` / `localize` machinery.
///
/// The bridge invokes this only when an override sink is
/// explicitly installed (tests, ops experiments); the production
/// bnetd path keeps the legacy reply ownership, which is per-connection
/// and depends on the legacy `localize()` table not reachable from the
/// application layer.
///
/// Why a port and not a free function?
///   - Reply text is locale-sensitive (the legacy table chooses by
///     the *sender's* clienttag/locale). The sink needs the caller's
///     identity, which the use-case already passes.
///   - Tests want to assert "what reply did the bridge emit?" with
///     zero legacy globals in play. A port + a recording fake is the
///     cheapest seam that achieves both.

#include <string_view>

#include "application/chat/whisper_use_case.hpp"

namespace pvpgn::application::chat {

/// Categorical reason the bridge is asking the sink to send a
/// user-visible reply. Mirrors the `WhisperVerdict` rejection arms;
/// the `Delivered` verdict is never funnelled here (delivery handles
/// its own ack path).
enum class WhisperReplyReason {
    NoTarget,
    EmptyBody,
    SelfWhisper,
    TargetOffline,
    TargetDnd,
    IgnoredBySender,
    IgnoredByTarget,
};

inline WhisperReplyReason verdict_to_reply_reason(WhisperVerdict v) noexcept {
    switch (v) {
        case WhisperVerdict::NoTarget:        return WhisperReplyReason::NoTarget;
        case WhisperVerdict::EmptyBody:       return WhisperReplyReason::EmptyBody;
        case WhisperVerdict::SelfWhisper:     return WhisperReplyReason::SelfWhisper;
        case WhisperVerdict::TargetOffline:   return WhisperReplyReason::TargetOffline;
        case WhisperVerdict::TargetDnd:       return WhisperReplyReason::TargetDnd;
        case WhisperVerdict::IgnoredBySender: return WhisperReplyReason::IgnoredBySender;
        case WhisperVerdict::IgnoredByTarget: return WhisperReplyReason::IgnoredByTarget;
        case WhisperVerdict::Delivered:       return WhisperReplyReason::SelfWhisper;  // unreachable
    }
    return WhisperReplyReason::NoTarget;
}

/// Read-only context the sink needs to pick a locale-appropriate
/// reply string. The bridge passes the *sender* fields here -- the
/// reply goes back to the sender, not the target.
struct WhisperReplyContext {
    std::string_view sender_name;
    std::string_view sender_clienttag;  // "STAR" / "WAR3" / ...
    std::string_view sender_locale;     // "enUS" / "ruRU" / ... (may be empty)
    std::string_view target_name;
};

class IChatReplySink {
public:
    virtual ~IChatReplySink() = default;

    /// Send a user-visible rejection reply to the sender. The
    /// implementation owns the locale lookup, the text format, and
    /// the transport. Returns true if the reply was queued for the
    /// sender; false if the implementation chose not to send
    /// (e.g. unknown reason, suppression policy).
    virtual bool emit_whisper_reply(
        WhisperReplyReason reason,
        const WhisperReplyContext& ctx) noexcept = 0;
};

/// Null sink: every call returns false. Useful default when no
/// production reply transport is wired yet.
class NullChatReplySink final : public IChatReplySink {
public:
    bool emit_whisper_reply(WhisperReplyReason,
                            const WhisperReplyContext&) noexcept override {
        return false;
    }
};

}  // namespace pvpgn::application::chat
