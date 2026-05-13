// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file events.hpp
/// Domain events — the *only* output of aggregate command methods. The
/// Application layer drains them after each command and publishes them
/// onto `IEventBus` for subscribers (script host, websocket hub,
/// metrics, audit log, integration tests).
///
/// Events are immutable value types with no behaviour. New events go
/// here, into the `DomainEvent` variant.

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "core/clock.hpp"
#include "domain/shared/ban.hpp"
#include "domain/shared/chat_message.hpp"
#include "domain/shared/client_tag.hpp"
#include "domain/shared/ids.hpp"
#include "domain/shared/ip_address.hpp"
#include "domain/shared/match_report.hpp"
#include "domain/shared/user_name.hpp"

namespace pvpgn::domain::events {

struct AccountCreated {
    AccountId id;
    UserName  name;
};

struct UserLoggedIn {
    AccountId        id;
    IpAddress        ip;
    ClientTag        tag;
    core::SystemTime at;
};

struct UserLoginRejected {
    enum class Reason : std::uint8_t {
        InvalidCredentials,
        AccountBanned,
        AccountLocked,
    };
    AccountId        id;
    Reason           reason;
    core::SystemTime at;
};

struct UserLoggedOut {
    AccountId id;
};

struct AccountPasswordChanged {
    AccountId id;
};

struct AccountCommandGroupGranted {
    AccountId    id;
    std::uint8_t group;        // 1..8 in legacy `command_groups.conf`
};

struct AccountBanned {
    AccountId id;
    Ban       ban;
};

struct AccountUnbanned {
    AccountId id;
};

// --- chat ---------------------------------------------------------------

struct ChannelJoined {
    ChannelId channel;
    AccountId who;
};

struct ChannelLeft {
    ChannelId channel;
    AccountId who;
};

struct ChannelJoinRejected {
    enum class Reason : std::uint8_t { Full, Banned, WrongClientTag, Locked };
    ChannelId channel;
    AccountId who;
    Reason    reason;
};

struct ChannelMessageSent {
    ChannelId   channel;
    AccountId   from;
    ChatMessage body;
};

struct ChannelTopicChanged {
    ChannelId   channel;
    AccountId   moderator;
    std::string topic;
};

struct ChannelMemberKicked {
    ChannelId channel;
    AccountId moderator;
    AccountId target;
};

// --- social -------------------------------------------------------------

struct FriendAdded {
    AccountId owner;
    AccountId target;
};

struct FriendRemoved {
    AccountId owner;
    AccountId target;
};

struct ClanCreated {
    ClanId    clan;
    AccountId founder;
};

struct ClanMemberJoined {
    ClanId    clan;
    AccountId who;
};

struct ClanMemberLeft {
    ClanId    clan;
    AccountId who;
};

// --- gameplay -----------------------------------------------------------

struct GameCreated {
    GameId    game;
    AccountId host;
    ClientTag client;
};

struct GameStarted {
    GameId           game;
    core::SystemTime at;
};

struct GamePlayerJoined {
    GameId    game;
    AccountId who;
};

struct GamePlayerLeft {
    GameId    game;
    AccountId who;
};

struct GameEnded {
    GameId      game;
    MatchReport report;
};

// --- moderation ---------------------------------------------------------

struct IpBanAdded {
    IpAddress                       ip;
    std::string                     reason;
    AccountId                       issuer;
    std::optional<core::SystemTime> expires_at;
};

struct IpBanRemoved {
    IpAddress ip;
};

struct IpBanRangeAdded {
    IpAddress                       network;
    std::uint8_t                    prefix_bits;
    std::string                     reason;
    AccountId                       issuer;
    std::optional<core::SystemTime> expires_at;
};

struct IpBanRangeRemoved {
    IpAddress    network;
    std::uint8_t prefix_bits;
};

struct AccountQuotaExceeded {
    AccountId id;
    std::uint32_t window_ms;
    std::uint32_t limit;
};

// --- social (team) ------------------------------------------------------

struct TeamCreated {
    TeamId                 team;
    std::vector<AccountId> members;
    ClientTag              client;
};

struct TeamDisbanded {
    TeamId team;
};

// --- matchmaking --------------------------------------------------------

struct AnonGameQueued {
    AccountId        account;
    ClientTag        client;
    std::uint8_t     team_size;
    core::SystemTime at;
};

struct AnonGameDequeued {
    AccountId account;
};

struct AnonGameMatched {
    GameId                 game;
    std::vector<AccountId> players;
    ClientTag              client;
};

struct TournamentScheduled {
    std::uint32_t    tournament_id;
    ClientTag        client;
    core::SystemTime start_at;
};

// --- realm --------------------------------------------------------------

struct RealmRegistered {
    std::uint32_t realm_id;
    std::string   name;
};

struct RealmUnregistered {
    std::uint32_t realm_id;
};

struct CharacterCreated {
    std::uint32_t realm_id;
    AccountId     owner;
    std::string   name;
};

struct CharacterDeleted {
    std::uint32_t realm_id;
    AccountId     owner;
    std::string   name;
};

// --- attributes (legacy bag) -------------------------------------------

struct AccountAttributeChanged {
    AccountId   id;
    std::string key;
    std::string value;
};

using DomainEvent = std::variant<
    AccountCreated,
    UserLoggedIn,
    UserLoginRejected,
    UserLoggedOut,
    AccountPasswordChanged,
    AccountCommandGroupGranted,
    AccountBanned,
    AccountUnbanned,
    ChannelJoined,
    ChannelLeft,
    ChannelJoinRejected,
    ChannelMessageSent,
    ChannelTopicChanged,
    ChannelMemberKicked,
    FriendAdded,
    FriendRemoved,
    ClanCreated,
    ClanMemberJoined,
    ClanMemberLeft,
    GameCreated,
    GameStarted,
    GamePlayerJoined,
    GamePlayerLeft,
    GameEnded,
    IpBanAdded,
    IpBanRemoved,
    IpBanRangeAdded,
    IpBanRangeRemoved,
    AccountQuotaExceeded,
    TeamCreated,
    TeamDisbanded,
    AnonGameQueued,
    AnonGameDequeued,
    AnonGameMatched,
    TournamentScheduled,
    RealmRegistered,
    RealmUnregistered,
    CharacterCreated,
    CharacterDeleted,
    AccountAttributeChanged
>;

}  // namespace pvpgn::domain::events
