# Bug Hunt: Warcraft III protocol (SID_WARCRAFTGENERAL / 0x44, w3route, W3 profile/icon)

ORIGINAL: `/home/cnupt/work/pvpgn-server`
V3:       `/home/cnupt/work/pvpgn`

Scope assigned: SID_WARCRAFTGENERAL (0x44) subcommand dispatch, the W3 route
protocol (w3route: host/join token, game id, header), W3 clan packets, W3
profile/stats request, W3 icon (GET_ICON / SET_ICON).

> NOTE ON THE ORIGINAL FORK. The pvpgn-server fork under comparison does **not**
> use a one-byte SID named `SID_WARCRAFTGENERAL`. Its bnet header is
> `bn_short type; bn_short size;` (a 16-bit LE type). The WAR3 anongame /
> profile / icon family is therefore `0x44ff` (= bytes `FF 44` on the wire),
> defined in `src/common/anongame_protocol.h`. The byte after `0xFF` (=`0x44`)
> is what the modern BNCS framing calls the SID. So **v3's `kWarcraftGeneral =
> 0x44` and pvpgn-server's `CLIENT_FINDANONGAME = 0x44ff` describe the same wire
> packet** — they MATCH at the wire level. The "WCSID_*" naming in the task
> brief comes from a different PvPGN lineage; here the per-subcommand selector is
> the `bn_byte option` field, the first byte of the packet body. Findings below
> use the pvpgn-server names.

Cross-references: `anongame.md` (INFOREPLY tag composition), `tournament-gameresult.md`,
`anongame-matchmaking.md` cover adjacent material; this file focuses on the
0x44 subcommand dispatch, route-token handling, and icon/profile replies.

---

## What MATCHES

- **0x44 SID value / wire framing.** v3 `sid::kWarcraftGeneral = 0x44`
  (`connection_fsm.hpp:130`) corresponds to the byte that follows `0xFF` in the
  BNCS header (`tcp_connection_context.hpp:46-49` writes `FF <sid> <len16>`).
  pvpgn-server stores the same two bytes as `bn_short type = 0x44ff`
  (`anongame_protocol.h:35`). Same wire bytes. CORRECT.

- **Subcommand selector is byte[0] of the body.** v3 reads
  `subcommand = payload[0]` (comment at `connection_fsm_ingame.cpp:83`); the
  original dispatches on `bn_byte_get(packet->u.client_anongame.option)` which
  is the first body byte after the 4-byte header
  (`handle_anongame.cpp:996`, struct `t_client_findanongame.option` at
  `anongame_protocol.h:125`). Reading position MATCHES.

- **w3route 16-bit type codes.** Every constant in
  `w3route_wire_types.hpp` matches `bnet_protocol.h`:
  kClientReq 0x1ef7 / kClientLoadingDone 0x23f7 / kServerReady 0x14f7 /
  kClientAbort 0x21f7 / kServerLoadingAck 0x08f7 / kClientConnected 0x3bf7 /
  kServerEchoReq 0x01f7 / kClientEchoReply 0x46f7 / kClientGameResult 0x2ef7 /
  kClientGameResultW3xp 0x3af7 / kServerAck 0x04f7 / kServerPlayerInfo 0x06f7 /
  kServerLevelInfo 0x47f7 / kServerStartGame1 0x0af7 / kServerStartGame2 0x0bf7.
  kGameResultLoss 0x3 / kGameResultWin 0x4 and kServerAckUnknown3 0x484e2637
  also match (`bnet_protocol.h:226-227, 300`). Constants CORRECT.

- **W3 GET_ICON table geometry.** `icon_table.hpp` mirrors the legacy
  `_client_anongame_get_icon`: WAR3 = 5 wide x 4 high, W3XP = 6 wide x 5 high
  (last column = tourney); race chars `R H O U N D`; race codes
  RANDOM/HUMANS/ORCS/UNDEAD/NIGHTELVES/DEMONS; row j -> icon char '2'+j.
  Matches `handle_anongame.cpp:466-486`. CORRECT (computation only — see
  Finding 3 re: no wire codec).

---

## Finding 1 — `on_warcraft_general` mislabels the `count` field as a "route token" and discards the whole subcommand family

- Severity: HIGH
- Classification: BUG (implemented-but-wrong; the rest is NOT-IMPLEMENTED)

The v3 handler treats body bytes `[1..4]` as a 4-byte "WAR3 route token" and
stores it via `set_war3_route_token`. In the real `0x44ff` protocol those four
bytes are the `bn_int count` field (a click-counter the server echoes back in
its reply), NOT a routing token. There is no route token anywhere in this packet
family; w3route tokens live on a *separate* connection class (`0xf7` types, a
dedicated TCP listener on port 6200), not in `0x44`.

ORIGINAL `src/common/anongame_protocol.h:122-134` — body layout after header:
```c
typedef struct {
    t_bnet_header h;
    bn_byte  option;     /* byte[0]  — subcommand selector            */
    bn_int   count;      /* byte[1..4] — "Goes up each time client clicks search" */
    bn_int   unknown2;
    bn_byte  type;       /* 0=PG 1=AT 2=TY */
    bn_byte  gametype;   /* 0=1v1 1=2v2 ... */
    bn_int   map_prefs;
    bn_byte  unknown3;   /* 8 */
    bn_int   id;         /* client id */
    bn_int   race;
} t_client_findanongame;
```
Dispatch `src/bnetd/handle_anongame.cpp:996-1027`:
```c
switch (bn_byte_get(packet->u.client_anongame.option)) {
  case CLIENT_FINDANONGAME_PROFILE:        return _client_anongame_profile(c, packet);
  case CLIENT_FINDANONGAME_CANCEL:         return _client_anongame_cancel(c);
  case CLIENT_FINDANONGAME_SEARCH:
  case CLIENT_FINDANONGAME_AT_INVITER_SEARCH:
  case CLIENT_FINDANONGAME_AT_SEARCH:      return handle_anongame_search(c, packet);
  case CLIENT_FINDANONGAME_GET_ICON:       return _client_anongame_get_icon(c, packet);
  case CLIENT_FINDANONGAME_SET_ICON:       return _client_anongame_set_icon(c, packet);
  case CLIENT_FINDANONGAME_INFOS:          return _client_anongame_infos(c, packet);
  case CLIENT_ANONGAME_TOURNAMENT:         return _client_anongame_tournament(c, packet);
  case CLIENT_FINDANONGAME_PROFILE_CLAN:   return _client_anongame_profile_clan(c, packet);
  default: ... "got unhandled option";
}
```
Option values (`anongame_protocol.h:45-54`):
SEARCH 0x00, INFOS 0x02, CANCEL 0x03, PROFILE 0x04, AT_SEARCH 0x05,
AT_INVITER_SEARCH 0x06, TOURNAMENT 0x07, PROFILE_CLAN 0x08, GET_ICON 0x09,
SET_ICON 0x0A.

V3 `src/application/connection/src/connection_fsm_ingame.cpp:80-99`:
```cpp
core::Status<> ConnectionFsm::on_warcraft_general(std::span<const std::byte> payload) {
    if (payload.size() < 5) return core::ok();
    // The route token is at bytes [1..4] (LE uint32) ...
    const std::uint32_t token = read_le32(payload, 1);
    set_war3_route_token(token);
    return core::ok();
}
```
The comment even claims "WID_GAMESEARCH = 0x00, WID_CANCELSEARCH = 0x01" — those
WID/WCSID names belong to another PvPGN lineage and do not match this fork's
option values (CANCEL is 0x03 here, not 0x01).

Divergence:
1. The 4 bytes read are `count`, not a token. Storing them as a route token is
   semantically wrong and meaningless.
2. `payload[0]` (the option) is never inspected. PROFILE/SEARCH/GET_ICON/
   SET_ICON/INFOS/TOURNAMENT/PROFILE_CLAN/CANCEL are all silently dropped — no
   reply is ever sent. A W3 client clicking a profile, requesting icons, or
   starting matchmaking gets no response.
3. The `< 5` guard happens to be harmless (the smallest real body is larger),
   but it is guarding the wrong thing.

Proposed fix: dispatch on `payload[0]` (option byte) into per-subcommand
handlers that decode the documented bodies and build the documented replies
(see Findings 2–4). Remove the bogus route-token extraction entirely; if a
w3route token mechanism is needed it belongs to the w3route connection class,
not here. At minimum, rename/repurpose so `count` is parsed correctly and
echoed in replies.

---

## Finding 2 — W3 PROFILE (option 0x04) and PROFILE_CLAN (0x08) replies are not implemented

- Severity: MEDIUM
- Classification: NOT-IMPLEMENTED

ORIGINAL builds `SERVER_FINDANONGAME_PROFILE` (0x44ff) in
`_client_anongame_profile` / `_client_anongame_profile_clan`
(`handle_anongame.cpp:160-310`, `79-130`). Reply header fields, in order:
`bn_byte option` (echo 0x04 / 0x08), `bn_int count` (echo client's count),
`bn_int icon` (`account_icon_to_profile_icon(...)`), `bn_byte rescount`, then
per-record stats. Struct: `t_server_findanongame_profile2`
(`anongame_protocol.h:347-355`), `t_server_findanongame_profile_clan`
(`:404-413`).

V3: no codec, no handler. `connection_fsm_ingame.cpp` drops these options
(Finding 1). The `icon_table` module computes race-icon *levels* but there is
no profile-stats reply builder and no `account_icon_to_profile_icon` equivalent
wired to a 0x44 reply.

Divergence: feature absent. Proposed fix: implement option 0x04 / 0x08
handlers + encoders matching the `option / count / icon / rescount / records`
layout. Mark clearly as not-yet-ported if intentional for the current
milestone.

---

## Finding 3 — W3 GET_ICON (0x09) / SET_ICON (0x0A) reply not wired to the protocol

- Severity: MEDIUM
- Classification: NOT-IMPLEMENTED (computation present, wire path absent)

ORIGINAL `_client_anongame_get_icon` (`handle_anongame.cpp:455-540+`) builds
`SERVER_FINDANONGAME_ICONREPLY` (0x44ff): `bn_int count`,
`bn_byte option`(=0x09), `curricon` (4 bytes, from
`account_get_user_icon` or `"%1d%c3W"` fallback / custom-icons), then the icon
table rows. `_client_anongame_set_icon` (option 0x0A) validates the requested
icon against the player's race-wins and persists it.

V3: `application/icon_table` computes the per-cell icon codes / unlock state
(a faithful port of the table math — see "What MATCHES"), but nothing decodes
the 0x09/0x0A request body, builds the ICONREPLY packet, or persists a SET_ICON
selection. `on_warcraft_general` drops these options.

Divergence: the hard part (table math) is ported but disconnected from the
wire. Proposed fix: add the GET_ICON/SET_ICON request decode + ICONREPLY
encode and call `icon_table` from there.

NOTE separate from this: `_client_iconreq` (the BNI-file request,
`CLIENT_ICONREQ 0x2dff` -> `SERVER_ICONREPLY` with `bn_long timestamp` +
filename, `handle_bnet.cpp:1270-1302`) is a *different* icon mechanism (the
`icons.bni` / `war3icons.bni` file). It is unrelated to the 0x44 per-account
stat icons and is out of the 0x44 scope; flagged here only to disambiguate the
two "icon" paths.

---

## Finding 4 — w3route protocol: constants only, no codec or handler

- Severity: MEDIUM
- Classification: NOT-IMPLEMENTED

`w3route_wire_types.hpp` (header comment confirms: "Constants-only; per-message
structs deferred until codec impl lands") defines the 15 type codes + 3 magic
constants and nothing else. A repo-wide search shows **zero** use of the
`w3route::` namespace outside the header — no decode/encode, no handler, no
connection class wiring. The original implements the full state machine in
`anongame.cpp::handle_w3route_packet` (`:1608+`), including:

- `CLIENT_W3ROUTE_REQ` (0x1ef7) body
  (`bnet_protocol.h:85-94`): `bn_int unknown1; bn_int id; bn_byte unknown2;
  bn_short port; bn_int handle;` then the player-name C-string. The handler reads
  the trailing `id` and external addr at offsets computed from
  `sizeof(t_client_w3route_req)+strlen(username)+2 (+12)`
  (`anongame.cpp:1664, 1684, 1690`).
- `SERVER_W3ROUTE_ACK` (0x04f7) with `playernum`, client `port`/`ip`
  (`bnet_protocol.h:283-297`; built at `anongame.cpp:1704`).
- `SERVER_W3ROUTE_PLAYERINFO` (0x06f7): `bn_int handle; bn_byte playernum;`
  then opponent name + `playerinfo2` (`unknown1; bn_int id; bn_int race`) +
  two `playerinfo_addr` blocks (external + LAN). `bnet_protocol.h:311-356`,
  built at `anongame.cpp:1900-1930`.
- LOADINGDONE/LOADINGACK/STARTGAME1/STARTGAME2/ECHO handshake.

These are exactly the "host/join token, game id, w3route header" structures the
brief asks about. None are decoded or produced by v3. Proposed fix: port
`handle_w3route_packet` (the actual byte-level offset arithmetic in
`anongame.cpp:1640-1940` is the load-bearing part to reproduce, especially the
manual offset reads past the variable-length username) plus the w3route
listener/connection class. Confirm whether the W3-route in-game relay is in
scope for the rewrite or intentionally deferred.

---

## Finding 5 — w3route game-result struct field order present in struct math but unverified end-to-end

- Severity: LOW
- Classification: UNSURE / NOT-IMPLEMENTED

The CLIENT_W3ROUTE_GAMERESULT (0x2ef7) / _W3XP (0x3af7) body in the original is a
variable-length sequence: `bn_byte number_of_results`, then N
`gameresult_player {number, result, race, unknown1, unknown2}`, then `part2`
(14 ints incl. unit/hero/resource scores), then M `hero {level(short),
race_and_name, hero_xp}`, then `part3` (10 ints)
(`bnet_protocol.h:212-275`). v3 has only the two type-code constants and the
win/loss result codes — no struct, no parser. Cross-ref
`tournament-gameresult.md` / `wol-gameres.md` for any partial result-recording
port. Proposed fix: when the w3route path is ported, reproduce this exact
nested layout; the `bn_short level` inside the hero record (the only non-int
field besides the leading `bn_byte`) is the easiest field to get wrong.

---

## Summary of classifications

| # | Area                              | Severity | Class            |
|---|-----------------------------------|----------|------------------|
| 1 | 0x44 `count` mislabeled as token; option byte ignored | HIGH | BUG |
| 2 | PROFILE / PROFILE_CLAN replies    | MEDIUM   | NOT-IMPLEMENTED  |
| 3 | GET_ICON / SET_ICON wire path     | MEDIUM   | NOT-IMPLEMENTED  |
| 4 | w3route protocol (whole class)    | MEDIUM   | NOT-IMPLEMENTED  |
| 5 | w3route gameresult struct         | LOW      | UNSURE/NOT-IMPL  |

Matches: 0x44 SID value & framing; subcommand selector = body byte[0];
all 15 w3route type codes + 3 magic constants; GET_ICON table geometry/race
maps. The implemented piece in scope (`on_warcraft_general`) is the only
implemented-but-wrong item (Finding 1); everything else is faithful-but-unused
constants or simply absent.
