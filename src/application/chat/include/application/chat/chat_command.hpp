// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file chat_command.hpp
/// CHAT vertical scaffold (Batch 17a, partial).
///
/// `classify_chat_command` is a pure function that takes the raw text
/// payload of a `CLIENT_CHATCOMMAND` (SID 0x0E) packet and decides
/// which logical chat action it represents. Today the legacy
/// `handle_command.cpp` interleaves this classification with the
/// effect (sending a whisper / executing a slash-command / forwarding
/// to channel members), which makes the routing logic untestable.
/// Splitting the classification out here gives the future
/// `application/chat/router` a deterministic, fuzz-friendly seam.
///
/// This is intentionally a SCAFFOLD ONLY: no bridge consumes it yet.
/// A follow-up batch (18a) wires it into a CHAT bridge.

#include <string>
#include <string_view>
#include <variant>

namespace pvpgn::application::chat {

/// `/w <target> <body>` (or `/whisper`, `/msg`, `/m`).
struct WhisperAction {
    std::string target;
    std::string body;
};

/// `/<name> [args...]`. `name` is lowercased ASCII; `args` is the raw
/// remainder verbatim (including any leading whitespace consumers
/// expect, post the single space separator).
struct CommandAction {
    std::string name;
    std::string args;
};

/// Plain channel chat (no leading slash).
struct ChannelMessageAction {
    std::string text;
};

/// Empty / whitespace-only payload — should be silently dropped.
struct EmptyAction {};

using ChatAction = std::variant<EmptyAction,
                                WhisperAction,
                                CommandAction,
                                ChannelMessageAction>;

/// Classify a raw chat-command payload.
///
/// The input is the NUL-stripped UTF-8 string the client sent.
/// Whitespace at both ends is trimmed before classification; the
/// trimmed string is what subsequent fields contain.
ChatAction classify_chat_command(std::string_view raw);

}  // namespace pvpgn::application::chat
