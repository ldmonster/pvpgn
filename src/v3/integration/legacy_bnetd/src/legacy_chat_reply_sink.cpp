// SPDX-License-Identifier: GPL-2.0-or-later
#include "integration/legacy_bnetd/legacy_chat_reply_sink.hpp"

#include <array>
#include <string>
#include <string_view>

#include "core/logging.hpp"
#include "integration/legacy_bnetd/bridge_logger.hpp"

namespace pvpgn::integration::legacy_bnetd {

namespace {

namespace pac = pvpgn::application::chat;

const char* key_for(pac::WhisperReplyReason r) noexcept {
    switch (r) {
        case pac::WhisperReplyReason::NoTarget:
            return "whisper.reply.no_target";
        case pac::WhisperReplyReason::EmptyBody:
            return "whisper.reply.empty_body";
        case pac::WhisperReplyReason::SelfWhisper:
            return "whisper.reply.self_whisper";
        case pac::WhisperReplyReason::TargetOffline:
            return "whisper.reply.target_offline";
        case pac::WhisperReplyReason::TargetDnd:
            return "whisper.reply.target_dnd";
        case pac::WhisperReplyReason::IgnoredBySender:
            return "whisper.reply.ignored_by_sender";
        case pac::WhisperReplyReason::IgnoredByTarget:
            return "whisper.reply.ignored_by_target";
    }
    return "whisper.reply.unknown";
}

// Default dispatch lives in a separate TU
// (`legacy_chat_reply_sink_default_dispatch.cpp`) which is only
// linked in alongside `bnetd_legacy`. When that TU is not in the
// link (e.g. unit tests against the unlinked variant), this slot
// stays `nullptr` until a test installs a stub via `set_dispatch`,
// keeping the sink free of legacy global dependencies at link time.
LegacyChatReplySink::DispatchFn g_dispatch = nullptr;

}  // namespace

void LegacyChatReplySink::set_dispatch(DispatchFn fn) noexcept {
    g_dispatch = fn;
}

bool LegacyChatReplySink::emit_whisper_reply(
    pac::WhisperReplyReason reason,
    const pac::WhisperReplyContext& ctx) noexcept {
    if (g_dispatch == nullptr) {
        bridge_log(core::LogLevel::Debug, "v3_chat_reply_sink",
                   "no dispatch installed; declining to reply");
        return false;
    }

    const char* key = key_for(reason);
    std::array<std::string_view, 2> args{ctx.target_name, ctx.sender_name};
    std::string text;
    try {
        text = tbl_.format(std::string_view{key},
                           ctx.sender_locale,
                           std::span<const std::string_view>{args});
    } catch (...) {
        // String-table impls promise noexcept; this is defensive.
        return false;
    }
    if (text.empty()) return false;

    const bool ok = g_dispatch(ctx.sender_name, text);
    if (!ok) {
        bridge_log(core::LogLevel::Debug, "v3_chat_reply_sink",
                   "dispatch returned false; falling back to legacy");
    }
    return ok;
}

}  // namespace pvpgn::integration::legacy_bnetd
