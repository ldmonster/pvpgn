// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file messages_friends.hpp
/// Friends and ArrangedTeam messages.
/// Part of the messages.hpp split — include messages.hpp for the full API.

#include <array>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include "messages/messages_common.hpp"

namespace pvpgn::protocol::bnet {

struct FriendsListRequest {
    bool operator==(const FriendsListRequest&) const = default;
};

/// Single entry inside a SERVER_FRIENDSLISTREPLY.
struct FriendsListEntry {
    std::string   name;           ///< friend account name
    std::uint8_t  status   = 0;   ///< FRIEND_TYPE_* bitfield
    std::uint8_t  location = 0;   ///< FRIENDSTATUS_*
    std::uint32_t client_tag = 0; ///< e.g. 'W3XP'
    std::string   location_name;  ///< channel/game name; "" if offline
    bool operator==(const FriendsListEntry&) const = default;
};

struct FriendsListReply {
    std::vector<FriendsListEntry> entries;
    bool operator==(const FriendsListReply&) const = default;
};

// --- SID_FRIENDINFO (0x66) — single-friend update --------------------------

struct FriendInfoRequest {
    std::uint8_t friend_num = 0;
    bool operator==(const FriendInfoRequest&) const = default;
};

struct FriendInfoReply {
    std::uint8_t  friend_num = 0;
    std::uint8_t  type       = 0;     ///< FRIEND_TYPE_*
    std::uint8_t  status     = 0;     ///< location code
    std::uint32_t client_tag = 0;
    std::string   game_name;          ///< empty if not in game
    bool operator==(const FriendInfoReply&) const = default;
};

// --- SID_FRIENDADD (0x67) / DEL (0x68) / MOVE (0x69) — server-only acks ----

/// 0x67 server → client. Wire layout matches `FriendsListEntry` (name, then
/// FRIEND_TYPE_* status byte, FRIENDSTATUS_* location byte, clienttag u32,
/// then trailing game/channel name cstring).
struct FriendAddAck {
    std::string   name;
    std::uint8_t  status     = 0;
    std::uint8_t  location   = 0;
    std::uint32_t client_tag = 0;
    std::string   location_name;
    bool operator==(const FriendAddAck&) const = default;
};

/// 0x68 server → client. Single byte: the slot index of the removed friend.
struct FriendDelAck {
    std::uint8_t friend_num = 0;
    bool operator==(const FriendDelAck&) const = default;
};

/// 0x69 server → client. Two slot indices being swapped in the roster.
struct FriendMoveAck {
    std::uint8_t pos1 = 0;
    std::uint8_t pos2 = 0;
    bool operator==(const FriendMoveAck&) const = default;
};

// --- SID_ARRANGEDTEAM_* (0x60..0x63, 0xFD) — W3 arranged-team handshake ---

/// 0x60 client → server: open the arranged-team friend screen. Empty body.
struct ArrangedTeamFriendScreenRequest {
    bool operator==(const ArrangedTeamFriendScreenRequest&) const = default;
};

/// 0x60 server → client: list of names eligible for arranged-team invites.
/// `f_count` byte prefixes a sequence of name cstrings.
struct ArrangedTeamFriendScreenReply {
    std::vector<std::string> names;
    bool operator==(const ArrangedTeamFriendScreenReply&) const = default;
};

/// 0x61 client → server: invite a set of friends into an arranged team.
/// `count` is the client's request cookie, `id` the client-supplied team
/// id, `unknown1` is always 0x00000001 on the wire.
struct ArrangedTeamInviteFriendRequest {
    std::uint32_t            count    = 0;
    std::uint32_t            id       = 0;
    std::uint32_t            unknown1 = 1;
    std::vector<std::string> friends;
    bool operator==(const ArrangedTeamInviteFriendRequest&) const = default;
};

/// 0x61 server → client: ack the invite request. `info[5]` is opaque
/// arranged-team metadata returned by bnetd.
struct ArrangedTeamInviteFriendAck {
    std::uint32_t                count     = 0;
    std::uint32_t                id        = 0;
    std::uint32_t                timestamp = 0;
    std::uint8_t                 team_size = 0;
    std::array<std::uint32_t, 5> info{};
    bool operator==(const ArrangedTeamInviteFriendAck&) const = default;
};

/// 0x62 server → client: notify the team that one invitee declined.
struct ArrangedTeamMemberDecline {
    std::uint32_t count       = 0;
    std::uint32_t action      = 0;
    std::string   decliner_name;
    bool operator==(const ArrangedTeamMemberDecline&) const = default;
};

/// 0x63 server → client: deliver an arranged-team invite to the invitee.
/// `inviter_ip` is a raw u32 in network order on the wire; we keep it as the
/// untouched u32 so the codec stays format-faithful. `port` is u16 LE.
struct ArrangedTeamSendInvite {
    std::uint32_t            count       = 0;
    std::uint32_t            id          = 0;
    std::uint32_t            inviter_ip  = 0;
    std::uint16_t            port        = 0;
    std::string              inviter_name;
    std::vector<std::string> other_names;
    bool operator==(const ArrangedTeamSendInvite&) const = default;
};

/// 0x63 client → server: invitee's reply. `option` is 3 (accept) or 2 (decline).
struct ArrangedTeamAcceptDeclineInvite {
    std::uint32_t count        = 0;
    std::uint32_t id           = 0;
    std::uint32_t option       = 0;
    std::string   inviter_name;
    bool operator==(const ArrangedTeamAcceptDeclineInvite&) const = default;
};

/// 0xFD client → server: invitee opened the invite dialog. Empty body.
struct ArrangedTeamAcceptInvite {
    bool operator==(const ArrangedTeamAcceptInvite&) const = default;
};

// --- SID_STARTADVEX3 (0x1C) — host game -----------------------------------

/// 0x1C client → server: open or update a hosted game advertisement.
struct StartGame4Request {
    std::uint16_t status   = 0;
    std::uint16_t flag     = 0;
    std::uint32_t unknown2 = 0;
    std::uint16_t gametype = 0;
    std::uint16_t option   = 0;
    std::uint32_t unknown4 = 0;
    std::uint32_t unknown5 = 0;
    std::string   game_name;
    std::string   password;
    std::string   info;
    bool operator==(const StartGame4Request&) const = default;
};

/// 0x1C server → client. `reply` = 0 on success.
struct StartGame4Ack {
    std::uint32_t reply = 0;
    bool operator==(const StartGame4Ack&) const = default;
};

// --- SID_UDPPINGRESPONSE (0x14) — UDP echo confirmation -------------------

/// 0x14 client → server. Sent by the client to confirm a successful UDP
/// echo round-trip; the four-byte echo equals what the server sent (usually
/// the ASCII tag `"tenb"`).
struct UdpOk {
    std::uint32_t echo = 0;
    bool operator==(const UdpOk&) const = default;
};

// --- SID_GETLADDERDATA (0x2E) — paginated ladder snapshot -----------------

/// 0x2E client → server: request a slice of a ladder.
struct LadderListRequest {
    std::uint32_t client_tag = 0;
    std::uint32_t id         = 0;   ///< 1 standard, 3 ironman
    std::uint32_t type       = 0;   ///< 0 highest-rated, 2 most-wins, 3 most-games
    std::uint32_t start      = 0;   ///< first row in the snapshot
    std::uint32_t count      = 0;   ///< number of rows requested
    bool operator==(const LadderListRequest&) const = default;
};

/// A `current` or `active` season block inside `LadderListEntry`.
struct LadderDataBlock {
    std::uint32_t wins        = 0;
    std::uint32_t loss        = 0;
    std::uint32_t disconnect  = 0;
    std::uint32_t rating      = 0;
    std::uint32_t rank        = 0;
    bool operator==(const LadderDataBlock&) const = default;
};

/// One row inside SERVER_LADDERREPLY.
/// `ttest[]` is treated as opaque six-u32 padding; clients are expected to
/// ignore the contents, but we preserve them so the encoder reproduces the
/// original wire exactly.
struct LadderListEntry {
    LadderDataBlock              current;
    LadderDataBlock              active;
    std::array<std::uint32_t, 6> ttest{};
    std::uint64_t                lastgame_current = 0;
    std::uint64_t                lastgame_active  = 0;
    std::string                  player_name;
    bool operator==(const LadderListEntry&) const = default;
};

/// 0x2E server → client. The header fields echo the request; `count` is the
/// number of entries that follow.
struct LadderListReply {
    std::uint32_t                 client_tag = 0;
    std::uint32_t                 id         = 0;
    std::uint32_t                 type       = 0;
    std::uint32_t                 start      = 0;
    std::uint32_t                 count      = 0;
    std::vector<LadderListEntry>  entries;
    bool operator==(const LadderListReply&) const = default;
};

// --- SID_CHECKAD (0x15) — banner request/reply ---------------------------

/// 0x15 client → server. `prev_adid` is the previously-displayed banner
/// (zero on the first request). `ticks` is a Unix-style timestamp.

}  // namespace pvpgn::protocol::bnet
