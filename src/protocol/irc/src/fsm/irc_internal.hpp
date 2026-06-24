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
///   :<server> <NNN> <nick> <params>
///
/// The @p nick is ALWAYS emitted as the implicit first parameter (mirroring the
/// original irc_send_cmd, irc.cpp:104). @p params is the handler-supplied
/// argument string that follows the nick; a leading ':' on any token marks the
/// start of the trailing parameter, exactly as on the wire. Callers must
/// therefore write @p params the way it appears after the nick, e.g.
///   make_numeric(srv, nick, 366, "#chan :End of /NAMES list")
///     => :srv 366 <nick> #chan :End of /NAMES list
Message make_numeric(std::string_view server, std::string_view nick, int code,
                     std::string_view params);

}  // namespace pvpgn::protocol::irc
