// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file legacy_message_sink.hpp
/// `IMessageSink` adapter delegating to legacy `message_send_text`.

#include "application/admin_commands/message_sink.hpp"

namespace pvpgn::integration::legacy_bnetd {

class LegacyMessageSink final
    : public application::admin_commands::IMessageSink {
public:
    void send(void* connection,
              Severity severity,
              std::string_view text) const override;
};

}  // namespace pvpgn::integration::legacy_bnetd
