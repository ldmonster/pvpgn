# Bug-hunt: Arranged Teams (AT games) — ORIGINAL vs v3

Subsystem: SID_ARRANGEDTEAM_* (0x60–0x63, 0xFD), team formation / invite / accept / decline,
team-size limits, team creation rules, AT ladder.

- ORIGINAL: `/home/cnupt/work/pvpgn-server`
- CURRENT v3: `/home/cnupt/work/pvpgn`

Key files:
- Original handlers: `src/bnetd/handle_bnet.cpp` `_client_atfriendscreen` / `_client_atinvitefriend` /
  `_client_atacceptdeclineinvite` / `_client_atacceptinvite` (lines 2479–2843)
- Original protocol: `src/common/anongame_protocol.h` (lines 475–574),
  `src/common/bnet_protocol.h:3706` (CLIENT_ARRANGEDTEAM_ACCEPT_INVITE 0xfdff)
- Original team model: `src/bnetd/team.cpp`, `src/bnetd/team.h` (MAX_TEAMSIZE 4)
- v3 wire IDs: `src/protocol/bnet/include/protocol/bnet/anongame_wire_types.hpp:35-121`,
  `.../messages/messages_common.hpp:38-42`
- v3 structs: `.../messages/messages_friends.hpp:80-149`
- v3 codec: `src/protocol/bnet/src/codec/codec_friends.cpp:109-360`, dispatch `src/protocol/bnet/src/codec.cpp:182-201,621-639`
- v3 FSM (stubs): `src/protocol/bnet/src/fsm/fsm_clan.cpp:65-76`
- v3 domain: `src/domain/social/include/domain/social/team.hpp`
- v3 use-cases: `src/application/social/src/create_team.cpp`, `disband_team.cpp`
- v3 repo port/impl: `src/domain/social/include/domain/social/ports.hpp:82-110`,
  `src/infra/inmemory/include/infra/inmemory/in_memory_team_repository.hpp`

---

## SUMMARY

The **wire/codec layer matches the original well** (SID values, packet field layout,
reply codes all correct). The **domain Team aggregate and create/disband use-cases exist but
diverge** from the original in two behavioral ways (no team reuse by member-set, ladder-stat
model entirely absent). The **entire server-side AT flow is NOT wired** — the FSM handlers are
stubs that only check login state and return ok(); they never call CreateTeam, never build a
friendscreen reply, never deliver invites or declines. So the feature is non-functional end-to-end
even though both the codec and the domain pieces are individually implemented.

---

## FINDING 1 — AT FSM handlers are stubs; no use-case is invoked
**Severity:** High (feature non-functional)
**Classification:** NOT-IMPLEMENTED

**Original** `src/bnetd/handle_bnet.cpp`:
- `_client_atfriendscreen` (2479): walks friend list + private-channel users, filters by
  mutual/online/same-version/not-dnd/not-away/in-channel, appends names, sets `f_count`, sends
  `SERVER_ARRANGEDTEAM_FRIENDSCREEN`.
- `_client_atinvitefriend` (2643): builds team via `account_find_team_by_accounts` / `create_team`,
  sets `currentatteam`, parts inviter from channel, sends `SERVER_ARRANGEDTEAM_SEND_INVITE` to each
  invitee, then `SERVER_ARRANGEDTEAM_INVITE_FRIEND_ACK` to the inviter with member UIDs.
- `_client_atacceptdeclineinvite` (2793): on decline, relays `SERVER_ARRANGEDTEAM_MEMBER_DECLINE` to
  the inviter.

**v3** `src/protocol/bnet/src/fsm/fsm_clan.cpp:65-76`:
```cpp
core::Status<> BnetFsm::on(const ArrangedTeamFriendScreenRequest&) {
    return require_clan_state(state_, "bnet fsm: AT_FRIENDSCREEN before login");
}
core::Status<> BnetFsm::on(const ArrangedTeamInviteFriendRequest&) {
    return require_clan_state(state_, "bnet fsm: AT_INVITE_FRIEND before login");
}
core::Status<> BnetFsm::on(const ArrangedTeamAcceptDeclineInvite&) { ... }
core::Status<> BnetFsm::on(const ArrangedTeamAcceptInvite&) { ... }
```
All four delegate to `require_clan_state` and return ok() — no friendscreen reply is produced, no
invite/decline packet is emitted, and `CreateTeam`/`DisbandTeam` are never called from the wire path.

**Divergence:** A real W3 client opening the AT friend screen / sending an invite receives no server
response. The team domain + use-cases are dead code from the protocol's perspective.

**Proposed fix:** Wire the handlers: friendscreen → enumerate eligible players and emit
`ArrangedTeamFriendScreenReply`; invite → call `CreateTeam` (or team-reuse lookup) and emit
`ArrangedTeamSendInvite` to invitees + `ArrangedTeamInviteFriendAck` to inviter; accept/decline →
emit `ArrangedTeamMemberDecline`. Track membership in find-by-members on the way.

---

## FINDING 2 — No team reuse by exact member set (stat continuity lost)
**Severity:** Medium
**Classification:** BUG (behavioral divergence in implemented use-case)

**Original** `src/bnetd/handle_bnet.cpp:2702-2709`:
```cpp
if (!(team = account_find_team_by_accounts(members[0], members, ctag))) {
    team = create_team(members, ctag);   // only create if no existing team
    ... "this team has never played before, creating new team"
} else {
    ... "this team has already played before"
}
```
Original `team.cpp:196-247` `_list_find_team_by_accounts` matches an existing team by the *exact set*
of accounts + clienttag + size, so the SAME roster keeps its `teamid`, wins/losses/xp/rank.

**v3** `src/application/social/src/create_team.cpp:13-47`: always allocates a fresh `TeamId` and
`Team::create(...)` — there is no "find existing team by members" path. The repository port
`ports.hpp:82-110` exposes `find_by_id` and `find_by_member` (single account → teams) but **no
find-by-account-set / find-by-uids** equivalent of the original.

**Divergence:** Re-forming the identical AT roster produces a brand-new team each time, so AT-ladder
history would not accumulate on the same team entity (once ladder is implemented). Also risks
duplicate team rows for one roster.

**Proposed fix:** Add `find_by_members(std::vector<AccountId>, ClientTag)` to `ITeamRepository`;
have `CreateTeam` look up an existing exact-set team first and return it instead of creating a new one.

---

## FINDING 3 — Team ID generation differs (timestamp vs monotonic counter; collision risk)
**Severity:** Medium
**Classification:** BUG / UNSURE (depends on whether IDs are meant to be stable keys)

**Original** `src/bnetd/team.cpp:43-90` `_cb_load_teams`: `team->teamid = ++max_teamid;` — a
monotonically increasing counter seeded from the max loaded id. IDs are unique and stable.

**v3** `src/application/social/src/create_team.cpp:22`:
```cpp
domain::TeamId team_id{static_cast<std::uint32_t>(std::time(nullptr))};
```
Team id = current unix time (seconds).

**Divergence:** Two teams created within the same second collide on id; the in-memory repo
`save()` (`in_memory_team_repository.hpp`) does `teams_[team.id()] = ...`, so a colliding id silently
overwrites the earlier team. Same strategy is acknowledged as copied from `CreateClan`, but for AT,
multiple teams forming concurrently in a busy server-second is plausible.

**Proposed fix:** Use a monotonic counter (seeded from repo max id) or a UUID/sequence, and/or have
the repo reject/handle id collisions rather than overwrite.

---

## FINDING 4 — AT ladder / team stats model entirely absent
**Severity:** Medium (feature gap)
**Classification:** NOT-IMPLEMENTED

**Original** `src/bnetd/team.cpp:371-540`: `t_team` carries wins, losses, xp, level, rank, lastgame;
`team_inc_wins`, `team_set_saveladderstats` (509) compute xp diff via `ladder_war3_xpdiff` and update
the AT ladder (`ladders.getLadderList(LadderKey(ladder_id_ateam, ...))`, line 538-540). The
`SERVER_ARRANGEDTEAM_INVITE_FRIEND_ACK.info[5]` carries member UIDs used by the later anongame search.

**v3** `src/domain/social/include/domain/social/team.hpp`: the `Team` aggregate models only
`id_`, `members_`, `client_`, `disbanded_`. No wins/losses/xp/level/rank/lastgame; no ladder update
path; `events.hpp` has only TeamCreated/TeamDisbanded (no team-result/stat events).

**Divergence:** AT ladder, team xp/level, and post-game stat persistence are not modeled at all.

**Proposed fix:** Out of scope for the wire bug-hunt, but flag: AT ladder requires extending the Team
aggregate with stat fields + a `record_result` method and an AT ladder repository, mirroring
`team_set_saveladderstats`.

---

## FINDING 5 — Teams are never persisted (in-memory only)
**Severity:** Medium
**Classification:** NOT-IMPLEMENTED

**Original** `team.cpp:160-187` `create_team` calls `storage->write_team(team)`; teams are loaded at
startup via `storage->load_teams(...)`, so AT teams + their ladder stats survive restarts.

**v3** `src/infra/mysql/src/unit_of_work.cpp:48` and `src/infra/sqlite/src/unit_of_work.cpp:34`
both back `teams()` with `inmemory::InMemoryTeamRepository` ("Teams are not yet persisted to SQL —
session-scoped"). No `SqlTeamRepository` exists.

**Divergence:** Any team created is lost on restart. Acceptable as a known gap, but it compounds
Findings 2/4 (no continuity is even possible without persistence).

**Proposed fix:** Add a SQL-backed `ITeamRepository` and wire it into the MySQL/SQLite UoWs.

---

## FINDING 6 — Stale/wrong SID comments in fsm_clan.cpp header
**Severity:** Low (doc only)
**Classification:** BUG (documentation)

**v3** `src/protocol/bnet/src/fsm/fsm_clan.cpp:20-23`:
```
///   on(ArrangedTeamAcceptDeclineInvite)  — SID_AT_ACCEPT_DECLINE (0x62)
///   on(ArrangedTeamAcceptInvite)         — SID_AT_ACCEPT_INVITE (0x63)
```
These are wrong. Per the original (`anongame_protocol.h`): client accept/decline is
`CLIENT_ARRANGEDTEAM_ACCEPT_DECLINE_INVITE = 0x63ff`, and `CLIENT_ARRANGEDTEAM_ACCEPT_INVITE = 0xfdff`
(`bnet_protocol.h:3706`). 0x62 is `SERVER_ARRANGEDTEAM_MEMBER_DECLINE` (server→client only).
The actual constants and the struct doc comments in `messages_friends.hpp:124-149` and the dispatch in
`codec.cpp:192-200` are CORRECT; only this header comment block is misleading.

**Proposed fix:** Correct the comment to `SID_AT_ACCEPT_DECLINE (0x63)` and
`SID_AT_ACCEPT_INVITE (0xFD)`.

---

## FINDING 7 — Codec friend/invite count limit (64) vs original team cap (3 invitees)
**Severity:** Low / INTENTIONAL
**Classification:** UNSURE (no functional bug today; over-permissive vs original)

**Original** `_client_atinvitefriend` `handle_bnet.cpp:2669-2672`:
```cpp
if ((count_to_invite < 1) || (count_to_invite > 3)) { ... return -1; }
```
Hard cap: 1..3 invitees → team size 2..4 (matches `MAX_TEAMSIZE 4`). The on-wire `info[]` and member
arrays are sized for exactly 4/5.

**v3** `codec_friends.cpp:111`: `kArrangedTeamFriendLimit = 64u` is used both for the invite
`numfriends` and friendscreen `f_count`. The friendscreen `f_count` legitimately can be large (it's a
list of eligible players, original uses a `uint8_t` up to 255), so 64 is fine there. But the **invite
`numfriends`** should be capped at 3 to match `MAX_TEAMSIZE` and the team-formation rule.

**Divergence:** The decoder would accept an AT invite with up to 64 friends; the original rejects >3.
Today this is harmless because the FSM handler (Finding 1) discards the message, but once invite is
wired, the use-case must enforce 1..3 (the domain `Team::create` does cap total members at 4 — see
Finding 8 MATCH — but the codec/use-case should reject early with the original's semantics).

**Proposed fix:** When wiring the invite handler, validate `friends.size()` in [1,3] (or rely on
`Team::create` rejecting >4 total). Optionally tighten the codec invite limit to a small value.

---

## WHAT MATCHES (verified correct)

1. **AT SID values** — all identical:
   - 0x60 friendscreen (`anongame_wire_types.hpp:35-36` vs `anongame_protocol.h:475,482`)
   - 0x61 invite-friend / ack (`:37-38` vs `:497,509`)
   - 0x62 member-decline server (`:39` vs `:549`)
   - 0x63 send-invite (server) / accept-decline (client) (`:40-41` vs `:521,535`)
   - 0xFD accept-invite (`messages_common.hpp:42` vs `bnet_protocol.h:3706`)
2. **Accept/decline reply codes** — `kClientArrangedTeamAccept=3`, `kClientArrangedTeamDecline=2`,
   `kServerArrangedTeamAccept=3`, `kServerArrangedTeamDecline=2` (`anongame_wire_types.hpp:116-119`)
   match original `:545-546,558-559`.
3. **Packet field layout** — all AT structs/codecs match the original wire format:
   - invite-friend req: count, id, unknown1(=1), numfriends(byte), names
     (`codec_friends.cpp:138-159` vs `anongame_protocol.h:497-506`)
   - invite-friend ack: count, id, timestamp, teamsize(byte), info[5]
     (`:161-177` / `:318-326` vs `:509-518`)
   - send-invite: count, id, inviter_ip, port(u16), numfriends(byte), inviter name, other names
     (`:189-216` / `:336-345` vs `:521-532`) — name ordering (inviter first) matches the original's
     append loop (`handle_bnet.cpp:2745-2748` appends members[0]=inviter first for non-self recipients)
   - member-decline: count, action, decliner name (`:179-187` / `:328-334` vs `:549-556`)
   - accept-decline: count, id, option, inviter name (`:218-227` / `:348-355` vs `:535-543`)
4. **Client dispatch** — `codec.cpp:192` correctly maps client SID 0x63 to
   `decode_arrangedteam_accept_decline_invite` (not send-invite), and 0xFD to accept-invite.
5. **Team size limits** — `Team::kMinSize=2`, `kMaxSize=4` (`team.hpp:28-29`) match original
   `MAX_TEAMSIZE=4` and the handler's 1..3-invitee rule (inviter + 1..3 = 2..4).
6. **Duplicate-member rejection** — `Team::create` rejects duplicate members (`team.hpp:38-45`); the
   original implicitly avoids this via distinct invitee lookups.
7. **Disband authority** — `DisbandTeam` requires the requester be a team member
   (`disband_team.cpp:22-24`). The original has no explicit disband packet for AT (teams are
   ephemeral/auto), so this is a reasonable equivalent; no conflicting original rule.
8. **Immutable roster** — v3 documents and enforces immutable membership (disband+recreate),
   matching original `create_team` semantics (members fixed at creation).
