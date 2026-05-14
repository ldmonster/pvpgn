// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file chat_reply_sink_override.hpp
/// Composition-root + test hook for `IChatReplySink` installed on
/// the chat strangler bridge (Batch 24a). Pass `nullptr` to clear.

#include "application/chat/chat_reply_sink.hpp"

namespace pvpgn::integration::legacy_bnetd {

/// Install (or clear with `nullptr`) the chat reply sink the chat
/// strangler bridge calls when a whisper verdict requires a
/// user-visible rejection reply. Not thread-safe; intended for
/// composition-root and test setup.
void set_chat_reply_sink_override(
    application::chat::IChatReplySink* sink) noexcept;

/// RAII guard mirroring `BridgeLoggerOverride`.
class ChatReplySinkOverride {
public:
    explicit ChatReplySinkOverride(
        application::chat::IChatReplySink& sink) noexcept {
        set_chat_reply_sink_override(&sink);
    }
    ~ChatReplySinkOverride() noexcept {
        set_chat_reply_sink_override(nullptr);
    }
    ChatReplySinkOverride(const ChatReplySinkOverride&)            = delete;
    ChatReplySinkOverride& operator=(const ChatReplySinkOverride&) = delete;
};

}  // namespace pvpgn::integration::legacy_bnetd
