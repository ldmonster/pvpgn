# Bug Hunt: BNCS Protocol Wire Codec (original vs v3)

Subsystem: BNCS protocol wire codec — packet structure, field offsets, byte
order, SID/EID constants, header framing.

ORIGINAL (read-only): `/home/cnupt/work/pvpgn-server`
CURRENT v3: `/home/cnupt/work/pvpgn`

## TL;DR

No wire-divergence bugs found. The v3 BNCS codec faithfully replicates the
original wire format across header framing, SID packet ids, EID chat-event
ids, integer widths + little-endian encoding, NUL-terminated strings, and
field ordering for every packet inspected. The areas most likely to break
real clients (header length computation, AUTH_INFO / AUTH_CHECK /
LOGON_RESPONSE2 layout, SID_CHATEVENT 6-int header + 2 strings, FRIENDSLIST
per-entry order, REALMLISTREPLY magic dwords) were all checked byte-for-byte
and MATCH.

Two LOW / informational notes are recorded below — neither is a wire bug.

---

## Coverage — constants verified to MATCH

### Header framing — MATCH
- Original `t_bnet_header { bn_short type; bn_short size }`
  (`src/common/bnet_protocol.h:41-45`) is on the wire
  `[u8 marker=0xFF][u8 code][u16 LE size]`, where `size` = TOTAL packet
  length including the 4-byte header.
- v3 `BnetHeader { u8 marker=0xFF; u8 code; u16 size }`
  (`src/protocol/common/include/protocol/common/packet.hpp:29-39`) decodes
  the same: marker checked == 0xFF, size read LE, `size < 4` rejected.
- v3 `Writer::finalize_bnet_packet()`
  (`src/protocol/common/include/protocol/common/writer.hpp:147-166`)
  back-patches `total = buf_.size() - bnet_start_` — i.e. header-inclusive
  total, matching the legacy `packet_set_size` semantics. LE throughout.
  No off-by-one: header is 4 bytes, payload excludes header on decode
  (`packet.hpp:106-108`).

### Single-byte SID codes — MATCH
v3 `src/protocol/bnet/include/protocol/bnet/messages/messages_common.hpp:17-123`
defines `kSid*` as the LOW byte of the legacy `0xNNff` type. Spot list of the
brief's required ids, all confirmed against `bnet_protocol.h`:

| SID | v3 kSid value | original | match |
|-----|--------------|----------|-------|
| AUTH_INFO | 0x50 | CLIENT_AUTH_INFO 0x50ff | ✓ |
| AUTH_CHECK | 0x51 | CLIENT_AUTHREQ_109 0x51ff | ✓ |
| LOGONRESPONSE (login1) | 0x29 | CLIENT_LOGINREQ1 0x29ff | ✓ |
| LOGONRESPONSE2 | 0x3A | CLIENT_LOGINREQ2 0x3aff | ✓ |
| ENTERCHAT | 0x0A | CLIENT_PLAYERINFOREQ/ENTERCHAT 0x0aff | ✓ |
| JOINCHANNEL | 0x0C | CLIENT_JOINCHANNEL 0x0cff | ✓ |
| CHATCOMMAND | 0x0E | CLIENT_MESSAGE 0x0eff | ✓ |
| CHATEVENT | 0x0F | SERVER_MESSAGE 0x0fff | ✓ |
| STARTADVEX3 | 0x1C | CLIENT/SERVER_STARTGAME4 0x1cff | ✓ |
| PING | 0x25 | CLIENT_ECHOREPLY / SERVER_ECHOREQ 0x25ff | ✓ |
| NULL | 0x00 | CLIENT_PINGREQ/SERVER_PINGREPLY 0x00ff | ✓ |
| GETADVLISTEX | 0x09 | GAMELISTREQ/REPLY 0x09ff | ✓ |
| GETFILETIME | 0x33 | FILEINFOREQ/REPLY 0x33ff | ✓ |
| CHANNELLIST | 0x0B | PROGIDENT2 / CHANNELLIST 0x0bff | ✓ |
| LEAVECHAT | 0x10 | CLIENT_LEAVECHANNEL 0x10ff | ✓ |

Whole-file cross-check of every `kSid*` (auth, chat, account, game, clan,
misc, realm, friends, arranged-team, legacy/OLS, game-lifecycle) against the
corresponding `0xNNff` `#define`s in `bnet_protocol.h` and
`anongame_protocol.h`: ALL MATCH. Notable extras confirmed:
- Friends 0x65-0x69 (`anongame_protocol.h:581-671`) → kSidFriendsList..Move ✓
- Arranged-team 0x60-0x63, 0xFD (`anongame_protocol.h:475-549`, `bnet_protocol.h:3706`) ✓
- Clan 0x70-0x82 (`bnet_protocol.h:3755-3695`) ✓
- WarcraftGeneral / anongame multiplexer 0x44 ✓

### Per-message wire-code constant headers — MATCH
`auth_wire_types.hpp`, `account_wire_types.hpp`, `chat_wire_types.hpp`,
`game_wire_types.hpp`, `clan_wire_types.hpp`, `misc_wire_types.hpp`,
`realm_wire_types.hpp`, `wire_types.hpp`, `init_wire_types.hpp` were each
diffed against the original `#define`s. Every `0xNNff` packet code, result
code, flag, and magic dword matched, including:
- CompInfo/CompReply magic: REG_AUTH 0xaa8843d1, CLIENT_ID 0x001b9dda,
  CLIENT_TOKEN 0xab69f79a ✓
- SESSIONKEY2 unknown 0x00004df3 ✓
- AUTHREPLY_109 OK/UPDATE/BADVERSION 0x00000000/0x00000100/0x00000101 ✓
- RegSnoop HKEYs 0x80000000-0x80000060 ✓
- LOGINREPLY2 SUCCESS/NONEXIST/BADPASS/LOCKED 0/1/2/6 ✓
- REALMLISTREPLY data unknowns (0xc0000000, 0x00018210, 0xffffffff, …)
  (`bnet_protocol.h:1271-1278`) ✓
- REALMLISTREPLY_110 data unknown1 0x00000001 ✓
- init connection classes (BNET 0x01, FILE 0x02, BOT 0x03, ENC 0x04,
  TELNET 0x0d, D2GS 0x64, D2CS_BNETD 0x65, LOCALMACHINE 0x98)
  (`init_protocol.h:59-67`) ✓

### EID chat-event ids — MATCH
v3 `chat_wire_types.hpp:49-63` vs original `bnet_protocol.h:2486-2508`:

| EID | value | match |
|-----|-------|-------|
| ADDUSER (SHOWUSER) | 0x01 | ✓ |
| JOIN | 0x02 | ✓ |
| PART (LEAVE) | 0x03 | ✓ |
| WHISPER | 0x04 | ✓ |
| TALK | 0x05 | ✓ |
| BROADCAST | 0x06 | ✓ |
| CHANNEL | 0x07 | ✓ |
| USERFLAGS | 0x09 | ✓ |
| WHISPERACK | 0x0a | ✓ |
| CHANNELFULL | 0x0d | ✓ |
| CHANNELDOESNOTEXIST | 0x0e | ✓ |
| CHANNELRESTRICTED | 0x0f | ✓ |
| INFO | 0x12 | ✓ |
| ERROR | 0x13 | ✓ |
| EMOTE | 0x17 | ✓ |

SERVER_MESSAGE magic dwords also MATCH: REG_AUTH 0xBAADF00D, ACCOUNT_NUM
0x0df0adba (`chat_wire_types.hpp:44-47` vs `bnet_protocol.h:2440-2444`).

### Field layout / byte order — MATCH (representative)
- SID_CHATEVENT (`codec/codec_chat.cpp:41-53, 126-137`): 6 LE u32
  (event_id, flags, ping/latency, user_ip, acct_number, registration) +
  2 NUL strings (username, text) == original `t_server_message`
  (`bnet_protocol.h:2428-2439`). ✓
- SID_AUTH_INFO (`codec/codec_auth.cpp:16-31, 209-223`): 9 LE u32 +
  2 NUL strings == original `t_client_auth_info`
  (`bnet_protocol.h:597-611`, fields protocol/archtag/clienttag/versionid/
  gamelang/localip/bias/lcid/langid + langstr + countryname). ✓
- SID_AUTH_INFO reply / AUTHREQ_109 (`codec_auth.cpp:41-66, 232-247`):
  logontype, server_token, session_num, u64 FILETIME (lo then hi, LE),
  mpq_filename, formula, optional W3 128-byte signature tail ==
  `t_server_authreq_109` (`bnet_protocol.h:783-792`). ✓
- SID_LOGONRESPONSE2 (`codec_auth.cpp:68-76, 249-256`): client_token,
  server_token, 5-dword password hash, username string. ✓
- SID_AUTH_CHECK request (`codec_auth.cpp:88-114, 268-290`): ticks,
  gameversion, checksum, cdkey_count, spawn, then per-cdkey
  {public, product, checksum, unknown, 5-dword hash} == `t_cdkey_info`
  (`bnet_protocol.h:922-929`), then exe_info + cdkey_owner strings. ✓
- SID_MESSAGEBOX (`codec_realm.cpp:41-48, 250-256`): style u32 + text +
  caption == `t_server_messagebox` (`bnet_protocol.h:4070-4076`). ✓
- SID_SERVERLIST (`codec_realm.cpp:31-37, 243-248`): unknown1 u32 + list
  string == `t_server_serverlist` (`bnet_protocol.h:2345-2351`). ✓
- SID_FRIENDSLIST reply (`codec_friends.cpp:15-41`): count byte, then per
  entry {name string, status byte, location byte, clienttag u32,
  location_name string}. Verified against the ACTUAL original builder in
  `handle_bnet.cpp:2324-2360` which appends name first, then status/
  location/clienttag — MATCH. (The struct comment at
  `anongame_protocol.h:593-600` listing "status, name, 6 unknown" is an
  imprecise legacy annotation, not the emitted order.)
- LE integers + NUL strings confirmed in the shared codec primitives:
  `Writer::write_le` / `write_cstring` (always appends one NUL)
  (`writer.hpp:54-83`); decode macros `RD_U16/U32/U64`, `RD_STR`
  (`codec/codec_internal.h:16-49`).

---

## Findings

### N1 — LOW / INFORMATIONAL — Two distinct wire-header models coexist (not a bug)
Classification: INTENTIONAL.
There are two header structs in v3:
- `pvpgn::protocol::bnet::BnetHeader` in
  `wire_types.hpp:26-30` — `{u16 type; u16 size}` (mirrors the legacy
  `t_bnet_header` struct shape, used only for static reference / the
  constants headers; structs are deferred).
- `pvpgn::protocol::BnetHeader` in
  `packet.hpp:31-39` — `{u8 marker; u8 code; u16 size}` (the actual codec
  framing).
These describe the SAME 4 wire bytes (`type` LE == marker byte 0xFF + code
byte). No divergence; noted only because a future reader might mistake the
former for the framing type. No action required.

### N2 — LOW / INFORMATIONAL — LoginReplyW3 BADACCT aliases ALREADY (faithfully replicated)
Classification: INTENTIONAL (matches original).
`account_wire_types.hpp:68-70` defines both
`kLoginReplyW3MessageAlready = 0x00000001` and
`kLoginReplyW3MessageBadAcct = 0x00000001` — the same collision exists in the
original header and is intentionally preserved. The v3 comment calls it out.
Not a wire bug.

---

## Method / scope notes
- Did NOT build or run; static comparison only.
- Cross-checked the full `kSid*` table, every `*_wire_types.hpp`, and the
  encode/decode bodies for auth, chat, friends, realm/serverlist/messagebox,
  and game-lifecycle modules. Game-report / anongame-routing (0x44 / w3route
  0xNNf7) sub-structs were spot-checked at the code-id level (all MATCH) but
  their large variable-length bodies were not exhaustively field-diffed; if a
  deeper pass is wanted, `codec_game.cpp`, `codec_w3.cpp`,
  `anongame/anongame_server.cpp`, and `w3route_wire_types.hpp` are the
  remaining surface.
- No BUG-class findings. No edits made.
