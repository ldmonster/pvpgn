// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
/// @file fsm_internal.hpp
/// Private shared declarations for BnetFsm sub-translation-units.
///
/// This header is NOT part of the public protocol/bnet API.  It lives in
/// src/protocol/bnet/src/fsm/ and is only included by the fsm/*.cpp files.
///
/// It exposes the require_clan_state() helper so that fsm_chat.cpp,
/// fsm_game.cpp, and fsm_clan.cpp can call it without duplicating the
/// implementation.  The definition lives in fsm.cpp (the thin coordinator).

#include "core/error.hpp"
#include "protocol/bnet/fsm.hpp"  // for BnetState

namespace pvpgn::protocol::bnet {

/// Guard helper: returns ok() when @p s is LoggedIn, InChat, or InGame;
/// otherwise returns a FailedPrecondition error with @p msg.
core::Status<> require_clan_state(BnetState s, const char* msg);

}  // namespace pvpgn::protocol::bnet
