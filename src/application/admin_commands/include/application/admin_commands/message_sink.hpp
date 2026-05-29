// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file message_sink.hpp
/// Port for sending a single chat-message text frame to a client.
/// Replaces direct calls to legacy `message_send_text` from
/// application-layer code.
///
/// Two severities are supported: the v3 router and responder maps
/// from semantic intent (informational / error) to the concrete
/// legacy message types (`message_type_info` / `message_type_error`)
/// in the adapter.

#include <string_view>

namespace pvpgn::application::admin_commands {

class IMessageSink {
public:
    virtual ~IMessageSink() = default;

    enum class Severity { Info, Error };

    virtual void send(void* connection,
                      Severity severity,
                      std::string_view text) const = 0;
};

}  // namespace pvpgn::application::admin_commands
