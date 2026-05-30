// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
/// @file irc_internal.hpp
/// Private shared helpers for IrcFsm sub-TUs.
///
/// Declares make_numeric() in the pvpgn::protocol::irc namespace so that
/// irc_registration.cpp and irc_commands.cpp can call it without duplicating
/// the implementation.  Defined in fsm.cpp (the thin coordinator).

#include <string_view>
#include <vector>

#include "protocol/irc/message.hpp"

namespace pvpgn::protocol::irc {

/// Build a numeric-reply Message following RFC 1459:
///   :<server> <NNN> <target> :<text>
Message make_numeric(std::string_view server, int code,
                     std::string_view target, std::string_view text);

}  // namespace pvpgn::protocol::irc
