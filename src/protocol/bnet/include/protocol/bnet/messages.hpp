// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file messages.hpp
/// Battle.net binary protocol messages — pure value types, no logic.
///
/// Deprecated forwarder: include specific sub-headers instead.
/// This file exists for backward compatibility; all existing
/// #include "protocol/bnet/messages.hpp" continue to work unchanged.
///
/// Sub-headers:
///   messages/messages_common.hpp        — SID constants, Null, Ping
///   messages/messages_auth.hpp          — AuthInfo, AuthCheck, LogonResponse2, CdKey2
///   messages/messages_chat.hpp          — JoinChannel, EnterChat, ChatCommand, ChatEvent
///   messages/messages_game.hpp          — GameList, LadderSearch, FileInfo
///   messages/messages_friends.hpp       — FriendsList, FriendInfo, ArrangedTeam
///   messages/messages_misc.hpp          — Ad, Motd, ChannelList, Profile, Email, Icon, etc.
///   messages/messages_realm.hpp         — RealmList, RealmJoin, WarcraftGeneral
///   messages/messages_clan.hpp          — Clan*
///   messages/messages_legacy.hpp        — Legacy/OLS protocol structs
///   messages/messages_game_lifecycle.hpp — CloseGame, StartGame, JoinGame, GameReport, MapAuth
///   messages/messages_variant.hpp       — ClientMessage, ServerMessage variants

#include "messages/messages_common.hpp"
#include "messages/messages_auth.hpp"
#include "messages/messages_chat.hpp"
#include "messages/messages_game.hpp"
#include "messages/messages_friends.hpp"
#include "messages/messages_misc.hpp"
#include "messages/messages_realm.hpp"
#include "messages/messages_clan.hpp"
#include "messages/messages_legacy.hpp"
#include "messages/messages_game_lifecycle.hpp"
#include "messages/messages_variant.hpp"
