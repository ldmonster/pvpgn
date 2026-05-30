// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file messages_chat.hpp
/// Chat messages: JoinChannel, EnterChat, ChatCommand, ChatEvent.
/// Part of the messages.hpp split — include messages.hpp for the full API.

#include <array>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include "messages/messages_common.hpp"

namespace pvpgn::protocol::bnet {

struct JoinChannel {
    std::uint32_t flags = 0;
    std::string   channel;
    bool operator==(const JoinChannel&) const = default;
};

/// SID_ENTERCHAT (client → server).
struct EnterChatRequest {
    std::string username;     ///< empty for product default
    std::string statstring;
    bool operator==(const EnterChatRequest&) const = default;
};

/// SID_ENTERCHAT (server → client).
struct EnterChatReply {
    std::string unique_name;
    std::string statstring;
    std::string account;
    bool operator==(const EnterChatReply&) const = default;
};

/// SID_CHATCOMMAND (client → server).
struct ChatCommand {
    std::string text;
    bool operator==(const ChatCommand&) const = default;
};

/// SID_CHATEVENT (server → client). Event IDs match legacy `EID_*`.
struct ChatEvent {
    std::uint32_t event_id     = 0;
    std::uint32_t flags        = 0;
    std::uint32_t ping_ms      = 0;
    std::uint32_t user_ip      = 0;
    std::uint32_t acct_number  = 0;
    std::uint32_t registration = 0;
    std::string   username;
    std::string   text;
    bool operator==(const ChatEvent&) const = default;
};

// --- SID_GETADVLISTEX (0x09) — public game list ----------------------------

/// SID_GETADVLISTEX (client → server).
/// `unknown1/2/3` track legacy fields; opaque to the codec.

}  // namespace pvpgn::protocol::bnet
