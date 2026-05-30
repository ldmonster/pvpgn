// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file messages_clan.hpp
/// Clan messages: ClanCreate, ClanDisband, ClanInvite, ClanMember, ClanMotd, ClanInfo.
/// Part of the messages.hpp split — include messages.hpp for the full API.

#include <array>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include "messages_common.hpp"

namespace pvpgn::protocol::bnet {

struct ClanCreateRequest {
    std::uint32_t cookie   = 0;
    std::uint32_t clan_tag = 0;       ///< 4-char clan tag, packed BE on wire (treated as u32 here)
    bool operator==(const ClanCreateRequest&) const = default;
};

/// SID_CLAN_CREATE (0x70) — reply: cookie + check_result + friend_count +
/// `friend_count` cstrings (members already-online and eligible to be invited).
struct ClanCreateReply {
    std::uint32_t            cookie       = 0;
    std::uint8_t             check_result = 0;
    std::vector<std::string> friend_names;
    bool operator==(const ClanCreateReply&) const = default;
};

/// SID_CLAN_DISBAND (0x73) — request: cookie only.
struct ClanDisbandRequest {
    std::uint32_t cookie = 0;
    bool operator==(const ClanDisbandRequest&) const = default;
};

/// SID_CLAN_MEMBERNEWCHIEF (0x74) — request: cookie + player_name.
struct ClanNewChiefRequest {
    std::uint32_t cookie = 0;
    std::string   player_name;
    bool operator==(const ClanNewChiefRequest&) const = default;
};

/// SID_CLAN_INVITE (0x77) — request: cookie + invitee_name.
struct ClanInviteRequest {
    std::uint32_t cookie = 0;
    std::string   player_name;
    bool operator==(const ClanInviteRequest&) const = default;
};

/// SID_CLANMEMBER_REMOVE (0x78) — request: cookie + player_name.
struct ClanMemberRemoveRequest {
    std::uint32_t cookie = 0;
    std::string   player_name;
    bool operator==(const ClanMemberRemoveRequest&) const = default;
};

/// SID_CLANMEMBER_RANKUPDATE (0x7A) — request: cookie + player_name + new_rank.
struct ClanMemberRankUpdateRequest {
    std::uint32_t cookie = 0;
    std::string   player_name;
    std::uint8_t  new_rank = 0;       ///< SERVER_CLAN_MEMBER_* (1=peon, 2=grunt, 3=shaman, 4=chieftain)
    bool operator==(const ClanMemberRankUpdateRequest&) const = default;
};

/// Generic cookie + result-byte reply used by 0x73, 0x74, 0x77, 0x78, 0x7A.
/// `sid` carries the SID code so the variant can disambiguate when needed;
/// equality only compares the wire payload (cookie, result).
struct ClanGenericResultReply {
    std::uint8_t  sid    = 0;
    std::uint32_t cookie = 0;
    std::uint8_t  result = 0;
    bool operator==(const ClanGenericResultReply&) const = default;
};

/// SID_CLAN_MOTDCHG (0x7B, client only) — unknown u32 + new MOTD cstring.
struct ClanMotdChange {
    std::uint32_t unknown1 = 0;
    std::string   motd;
    bool operator==(const ClanMotdChange&) const = default;
};

/// SID_CLAN_MOTD (0x7C) — request: cookie only.
struct ClanMotdRequest {
    std::uint32_t cookie = 0;
    bool operator==(const ClanMotdRequest&) const = default;
};

/// SID_CLAN_MOTD (0x7C) — reply: cookie + unknown u32 + motd cstring.
struct ClanMotdReply {
    std::uint32_t cookie   = 0;
    std::uint32_t unknown1 = 0;
    std::string   motd;
    bool operator==(const ClanMotdReply&) const = default;
};

// --- SID 0x71 / 0x72 / 0x79 — multi-cookie invite chains -----------------
//
// Creating a clan needs the founder to gather signatures from at least four
// friends. The wire conversation is:
//   client → 0x71 ClanCreateInviteRequest    (cookie A)
//   server → 0x72 ClanCreateInviteForward    (cookie B, to each friend)
//   client ← 0x72 ClanCreateInviteResponse   (cookie B, accept / decline)
//   server → 0x71 ClanCreateInviteSummary    (cookie A, status, failed name)
// After the clan exists, late joins use the simpler 0x79 pair.

/// 0x71 client → server: list of friends to enroll for a new clan.
struct ClanCreateInviteRequest {
    std::uint32_t            cookie   = 0;
    std::string              clan_name;
    std::uint32_t            clan_tag = 0;
    std::vector<std::string> friend_names;
    bool operator==(const ClanCreateInviteRequest&) const = default;
};

/// 0x71 server → client (founder): result of the enrollment poll.
/// `status`: 0x00 OK, 0x04 declined, 0x05 cannot-contact / already-in-clan.
/// `failed_member` carries the offending name when status != 0.
struct ClanCreateInviteSummary {
    std::uint32_t cookie = 0;
    std::uint8_t  status = 0;
    std::string   failed_member;     ///< empty on success
    bool operator==(const ClanCreateInviteSummary&) const = default;
};

/// 0x72 server → client (invited friend): forwarded poll question.
struct ClanCreateInviteForward {
    std::uint32_t            cookie       = 0;
    std::uint32_t            clan_tag     = 0;
    std::string              clan_name;
    std::string              clan_creator;
    std::vector<std::string> friend_names;
    bool operator==(const ClanCreateInviteForward&) const = default;
};

/// 0x72 client → server (invited friend): accept / decline.
/// `reply`: 0x04 decline, 0x05 cannot-contact / busy, 0x06 accept.
struct ClanCreateInviteResponse {
    std::uint32_t cookie       = 0;
    std::uint32_t clan_tag     = 0;
    std::string   clan_creator;
    std::uint8_t  reply        = 0;
    bool operator==(const ClanCreateInviteResponse&) const = default;
};

/// 0x79 server → client: late-join invite (clan already exists).
struct ClanInvite2Forward {
    std::uint32_t cookie    = 0;
    std::uint32_t clan_tag  = 0;
    std::string   clan_name;
    std::string   inviter_name;
    bool operator==(const ClanInvite2Forward&) const = default;
};

/// 0x79 client → server: late-join invite reply. `reply` uses the same codes
/// as `ClanCreateInviteResponse::reply`.
struct ClanInvite2Response {
    std::uint32_t cookie       = 0;
    std::uint32_t clan_tag     = 0;
    std::string   inviter_name;
    std::uint8_t  reply        = 0;
    bool operator==(const ClanInvite2Response&) const = default;
};

// --- SID_CLANMEMBERLIST (0x7D) / member-event notifies (0x7E, 0x7F) -------

/// 0x7D client → server: request the current clan member list. Cookie echoes
/// back in the reply.
struct ClanMemberListRequest {
    std::uint32_t cookie = 0;
    bool operator==(const ClanMemberListRequest&) const = default;
};

/// Single entry inside `ClanMemberListReply`. `rank` uses the SERVER_CLAN_MEMBER_*
/// constants (PEON=0x01 .. CHIEFTAIN=0x04, NEW=0x00). `online_status` uses the
/// SERVER_CLAN_MEMBER_OFFLINE/ONLINE/CHANNEL/GAME/PRIVATE_GAME constants.
/// `location` is the trailing string (channel/game name) — empty when offline.
struct ClanMemberEntry {
    std::string   name;
    std::uint8_t  rank          = 0;
    std::uint8_t  online_status = 0;
    std::string   location;
    bool operator==(const ClanMemberEntry&) const = default;
};

/// 0x7D server → client: the full clan roster.
struct ClanMemberListReply {
    std::uint32_t cookie = 0;
    std::vector<ClanMemberEntry> members;
    bool operator==(const ClanMemberListReply&) const = default;
};

/// 0x7E server → client: notify that a member was removed from the clan.
struct ClanMemberRemovedNotify {
    std::string name;
    bool operator==(const ClanMemberRemovedNotify&) const = default;
};

/// 0x7F server → client: update a member's online status.
/// `online_status` matches SERVER_CLAN_MEMBER_OFFLINE/ONLINE/CHANNEL/GAME/PRIVATE_GAME.
/// `location` is empty unless the member is in a channel or game.
struct ClanMemberUpdate {
    std::string   name;
    std::uint8_t  rank          = 0;
    std::uint8_t  online_status = 0;
    std::string   location;
    bool operator==(const ClanMemberUpdate&) const = default;
};

// --- SID_CLANINFO (0x82) — clan-membership query ---------------------------
struct ClanInfoRequest {
    std::uint32_t cookie   = 0;
    std::uint32_t clan_tag = 0;       ///< 4-char clan tag
    std::string   player_name;
    bool operator==(const ClanInfoRequest&) const = default;
};

/// On `fail == 0` the reply carries clan_name, rank, join_time.
/// On `fail != 0` the trailing fields are absent.
struct ClanInfoReply {
    std::uint32_t cookie    = 0;
    std::uint8_t  fail      = 0;
    std::string   clan_name;
    std::uint8_t  rank      = 0;
    std::uint32_t join_time = 0;
    bool operator==(const ClanInfoReply&) const = default;
};

// =========================================================================

}  // namespace pvpgn::protocol::bnet
