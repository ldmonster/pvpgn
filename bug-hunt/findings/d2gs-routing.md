# Bug-hunt: d2cs ↔ d2gs game-server routing protocol

Subsystem: the packets between the D2 char server (d2cs) and the D2 game
server (d2gs) — create game, join game, close game, update game info, the
GS auth/register handshake (opcodes 0x10–0x23 in
`src/common/d2cs_d2gs_protocol.h`).

ORIGINAL: `/home/cnupt/work/pvpgn-server`
CURRENT v3: `/home/cnupt/work/pvpgn`

## Executive summary

The d2cs↔d2gs routing **state machine** (forwarding a client CREATEGAMEREQ /
JOINGAMEREQ to a chosen game server, and handling the GS's
CREATEGAMEREPLY / JOINGAMEREPLY / UPDATEGAMEINFO / CLOSEGAME / AUTHREPLY /
SETGSINFO) **is NOT implemented in v3.** The v3 tree contains:

1. **Observation-only stubs** (`send_outbound_obs_bridges.cpp`,
   `handle_d2gs_packet_bridge.cpp`, `d2gs_bridge.cpp`) that just log and
   `return 0` ("fall through to legacy"). But there is **no legacy
   `handle_d2gs.cpp` / `handle_d2cs.cpp` in the v3 tree** to fall through to
   (`find . -name handle_d2gs.cpp` → nothing). So the routing logic is
   simply absent / pending.
2. A **parallel set of d2cs→d2gs encoders** in
   `send_handle_d2gs_bridges.cpp` (AUTHREQ 0x10, AUTHREPLY 0x11,
   SETGSINFO 0x12, SETINITINFO 0x15, SETCONFFILE 0x16). These are **byte-ports**
   and are correct (verified below), but they are **only exercised by unit
   tests** — `grep` shows no live caller wires them into any send path.
3. v3 wire-type mirrors in `protocol/d2gs/wire_types.hpp` (the d2cs↔d2gs
   structs) and `protocol/d2cs/wire_types.hpp` (the client↔d2cs structs).

The implemented byte layouts match the originals. The findings below record
(a) the NOT-IMPLEMENTED gaps that matter for behavior parity, and (b) the
verified-correct byte layouts (per task instructions to note matches), plus
(c) one latent correctness risk in the parallel encoders that will bite when
they are eventually wired up.

---

## Finding 1 — d2cs↔d2gs routing handlers NOT IMPLEMENTED in v3

- **Severity:** High (functional parity — D2 game create/join is broken if v3
  is meant to replace legacy here; benign if legacy d2cs binary is still
  shipped unchanged and v3 only observes).
- **Classification:** NOT-IMPLEMENTED
- **Original ref:**
  `src/d2cs/handle_d2gs.cpp` — full dispatch table + handlers
  `on_d2gs_authreply`, `on_d2gs_setgsinfo`, `on_d2gs_creategamereply`
  (line 270), `on_d2gs_joingamereply` (line 329), `on_d2gs_updategameinfo`
  (line 430), `on_d2gs_closegame` (line 465); plus
  `src/d2cs/handle_d2cs.cpp` `on_client_creategamereq` (line 222) and
  `on_client_joingamereq` (line 351) which build the CREATEGAMEREQ(0x20) /
  JOINGAMEREQ(0x21) to the GS.
- **v3 ref:** `src/app/d2cs/src/send_outbound_obs_bridges.cpp:20-25`
  (`pvpgn_v3_d2cs_obs_creategamereq_d2gs` / `..._joingamereq_d2gs` are empty
  `return 0`); `src/app/d2cs/src/handle_d2gs_packet_bridge.cpp:37`
  (`pvpgn_v3_d2cs_handle_d2gs_packet` only logs). No v3 handler builds the
  GS-bound CREATEGAMEREQ/JOINGAMEREQ, allocates the server-queue seqno,
  consumes the GS reply, or maps GS gameid back to the client.
- **Divergence:** The seqno→server-queue (`t_sq`) correlation, the
  `d2gslist_choose_server` load balancing, the game-queue fallback, the
  gameid allocation, the GS reply→client reply mapping — none of this exists
  in v3.
- **Proposed fix:** Port the routing FSM (or keep shipping the legacy d2cs
  binary). Until then the encoders in Finding 5 are dead code. Flag as a
  milestone gap, not a regression to "fix" blind.

---

## Finding 2 — GS-bound CREATEGAMEREQ/JOINGAMEREQ game-settings + address fields not byte-ported

- **Severity:** Medium (will be the bug surface when Finding 1 is addressed)
- **Classification:** NOT-IMPLEMENTED (no v3 encoder exists for 0x20/0x21
  in the CS→GS direction)
- **Original ref:** `src/d2cs/handle_d2cs.cpp:325-341` (creategamereq):
  ```
  packet_set_type(gspacket,D2CS_D2GS_CREATEGAMEREQ);        // 0x20
  bn_byte_set(...difficulty,difficulty);
  bn_byte_set(...hardcore,hardcore);
  bn_byte_set(...expansion,expansion);
  bn_byte_set(...ladder,ladder);
  packet_append_string(gspacket,gamename);  // then gamepass,gamedesc,
  packet_append_string(gspacket,d2cs_conn_get_account(c));  // acct,
  packet_append_string(gspacket,d2cs_conn_get_charname(c)); // char,
  addr.s_addr = htonl(d2cs_conn_get_addr(c));               // ip string
  inet_ntop(...); packet_append_string(gspacket,addrstr);
  ```
  and `:419-432` (joingamereq): type 0x21, `bn_int_set gameid`,
  `bn_int_set token`, then charname / account / ip-string.
- **v3 ref:** none. `protocol/d2gs/wire_types.hpp` *declares* the structs
  `CreateGameReq` (`ladder, expansion, difficulty, hardcore`, lines 159-167)
  and `JoinGameReq` (`gameid, token`, lines 177-183), and the byte order of
  the four settings bytes matches the original struct — but there is no
  `encode()` and no bridge that serializes gamename/gamepass/gamedesc/
  account/charname/ip-string trailer.
- **Note on the IP field:** original appends the client IP as a **decimal
  dotted string** via `inet_ntop` (NOT a binary big-endian int). Any future
  v3 encoder must emit a NUL-terminated ASCII string here, not a 4-byte addr.
  This is a likely future-bug trap.
- **Game-settings encoding caveat:** `difficulty/hardcore/expansion/ladder`
  passed to the GS are the *unpacked* per-field values
  (`gameflag_get_difficulty()` / `conn_get_charinfo_*`), NOT the packed
  `gameflag` word. The packed `gameflag` is computed by
  `gameflag_create(l,e,h,d) = 0x04 | (e?EXP:0) | (h?HC:0) | ((d&7)<<0x0c) |
  (l?LADDER:0)` (`src/d2cs/game.h:79`) and only used internally / toward the
  client. A v3 port must keep the two representations distinct.
- **Proposed fix:** When porting, mirror the original field-by-field;
  emit the IP as `inet_ntop` ASCII; send unpacked settings bytes in the
  order `ladder, expansion, difficulty, hardcore`.

---

## Finding 3 — JoinGameReply→client `addr` is big-endian; v3 encoder uses bswap (MATCHES, but coupling is fragile)

- **Severity:** Low (currently correct)
- **Classification:** BUG-AVOIDED / verify
- **Original ref:** `src/d2cs/handle_d2gs.cpp:417`
  `bn_int_nset(&rpacket->u.d2cs_client_joingamereply.addr,gsaddr);`
  — `bn_int_nset` writes **network/big-endian** (the GS IP the client
  connects to), whereas every other field uses host-LE `bn_int_set`.
  Struct: `src/common/d2cs_protocol.h:127-137` (`addr` is `bn_int`).
- **v3 ref:** `src/app/d2cs/src/send_joingamereply_bridge.cpp:39`
  `m.addr = bswap32(addr_host);` then the codec
  (`protocol/d2cs/src/codec.cpp:328`) writes `m.addr` **little-endian**.
  Swapping a host value then writing LE reproduces the original BE wire
  bytes. This matches the d2dbs-pass observation and is **correct**.
- **Divergence:** None on the wire. Risk: the correctness depends on the
  caller passing `addr_host` in host byte order. The comment is accurate.
- **Proposed fix:** None. Keep the comment; consider an explicit
  `write_be<uint32_t>` in the codec for `JoinGameReply.addr` to remove the
  swap-then-LE indirection (clarity only).

---

## Finding 4 — d2cs↔d2gs opcode values and header layout: VERIFIED MATCH

- **Severity:** n/a (positive verification, per task)
- **Classification:** MATCHES
- **Header:** original `t_d2cs_d2gs_header` = `bn_short size; bn_short type;
  bn_int seqno` (8 bytes, all LE) — `d2cs_d2gs_protocol.h:37-42`. v3
  `d2gs::wire::Header { uint16 size; uint16 type; uint32 seqno }`,
  `static_assert(sizeof==8)` — `protocol/d2gs/wire_types.hpp:21-28`. The
  `put_header` helper (`send_handle_d2gs_bridges.cpp:13-20`) writes
  size/type/seqno all LE. **Match.**
- **Opcodes** (`d2cs_d2gs_protocol.h` vs `wire_types.hpp:33-48`):
  AUTHREQ 0x10, AUTHREPLY 0x11 (both directions), SETGSINFO 0x12 (both),
  ECHOREQ/ECHOREPLY 0x13, CONTROL 0x14, SETINITINFO 0x15, SETCONFFILE 0x16,
  CREATEGAMEREQ/REPLY 0x20, JOINGAMEREQ/REPLY 0x21, UPDATEGAMEINFO 0x22,
  CLOSEGAME 0x23. **All match**, including the deliberate dual-direction
  reuse of 0x11/0x12/0x13/0x20/0x21.
- **Per-packet bodies** (all LE u32 unless noted), verified field-for-field:
  - AuthReq: sessionnum, signlen (+realm string) — matches
    `handle_d2gs.cpp:489-494`.
  - AuthReplyFromD2gs: version, checksum, randnum, signlen, sign[128]
    — matches struct `d2cs_d2gs_protocol.h:60-68` and read sites
    `handle_d2gs.cpp:181-188`.
  - AuthReplyFromD2cs (CS→GS): single `reply` u32 — matches `:75`.
  - SetGsInfo: maxgame, gameflag — matches.
  - SetInitInfo: time, gs_id, ac_version (+ac_checksum, +ac_string) —
    matches `handle_d2gs.cpp:102-111`.
  - SetConfFile: size, reserved1 (+raw file bytes) — matches `:137-141`
    (original sets `reserved1 = time()`).
  - CreateGameReply: result, gameid — matches.
  - JoinGameReply (GS→CS): result, gameid — matches.
  - UpdateGameInfo: flag, gameid, charlevel, charclass (+charname) —
    matches `handle_d2gs.cpp:444-448`.
  - CloseGame: gameid — matches.
- **Reply/flag constants** (`wire_types.hpp:50-84`): AUTHREPLY succeed/
  bad-version/bad-checksum 0/1/2; CONTROL restart/shutdown 1/2; difficulty
  normal/nm/hell 0/1/2; CREATEGAME succeed/failed 0/1; JOINGAME succeed/
  failed/full 0/1/2; UPDATEGAMEINFO update/enter/leave 0/1/2. **All match.**

---

## Finding 5 — Parallel d2gs encoders use `Writer` (unpacked), not the legacy packed struct — correct, but `AuthReplyFromD2gs.sign[128]` over-read risk if ever decoded

- **Severity:** Low (latent; no decoder exists yet)
- **Classification:** UNSURE / future-risk
- **Original ref:** `handle_d2gs.cpp:181-188` reads
  `version/checksum/randnum/signlen` and `sign` directly from the packed
  `t_d2gs_d2cs_authreply` (sign is `bn_basic sign[128]`, fixed). The handler
  trusts the 0x11 dispatch-table size guard
  `sizeof(t_d2gs_d2cs_authreply)` (`handle_d2gs.cpp:76`) so `sign[128]` is
  always present.
- **v3 ref:** `protocol/d2gs/wire_types.hpp:96-105` mirrors the struct with a
  fixed `std::array<uint8_t,128> sign`. No v3 **decoder** for the GS→CS
  AUTHREPLY exists yet (only the CS→GS reply is encoded in
  `send_handle_d2gs_bridges.cpp:56`). When a decoder is written, it must
  enforce the full `8 + 4*4 + 128 = 152`-byte minimum the same way the
  legacy size guard did, or `signlen`-driven reads could over-read.
- **Divergence:** None today (no decoder). Recorded so the size guard isn't
  dropped during the eventual port.
- **Proposed fix:** When adding the GS→CS AUTHREPLY decoder, bound-check the
  body to ≥152 bytes and treat `signlen` as advisory only.

---

## Finding 6 — Header `type` field width: original `bn_short` (2 bytes) vs handler-table indexing — MATCHES, note

- **Severity:** n/a
- **Classification:** MATCHES (note)
- The d2cs↔d2gs header `type` is a 2-byte LE short (`d2cs_d2gs_protocol.h:40`),
  unlike the client↔d2cs header where `type` is a 1-byte field
  (`d2cs_protocol.h` / `protocol/d2cs/wire_types.hpp:103-108`,
  `ClientHeader{ uint16 size; uint8 type }`, 3 bytes). v3 keeps these two
  header shapes distinct and correct: `d2gs::wire::Header` (8 bytes, u16
  type) vs `d2cs::wire::ClientHeader` (3 bytes, u8 type). The
  `send_handle_d2gs_bridges` `put_header` correctly writes type as LE16.
  No bug; flagged because mixing the two header widths is an easy mistake and
  v3 got it right.

---

## Client↔d2cs game packets (adjacent, verified for completeness)

These are the client-facing 0x03/0x04 packets that bracket the GS routing;
their v3 codec is live (FSM-driven) and matches the originals:

- `ClientCreateGameReq` (0x03): seqno(u16), gameflag(u32), u1(u8),
  leveldiff(u8), maxchar(u8) + gamename/pass/desc strings —
  `wire_types.hpp:163-174`, codec `codec.cpp:92-103` — matches
  `d2cs_protocol.h:87-99`.
- `CreateGameReply` (0x03): seqno, gameid, u1 (all u16), reply(u32) —
  `wire_types.hpp:176-186`, codec `:151-158` — matches `d2cs_protocol.h:101-109`.
- `ClientJoinGameReq` (0x04): seqno(u16) + gamename/pass — matches `:118-125`.
- `JoinGameReply` (0x04): seqno/gameid/u1 (u16), addr/token/reply (u32) —
  `wire_types.hpp:197-209`, codec `:159-168` — matches `:127-137`. The
  `addr` BE handling is Finding 3.
- Reply-code constants (`wire_types.hpp:62-81`) match the `D2CS_CLIENT_*`
  defines (creategame name-exist 0x1f, joingame full 0x2b, etc.).

---

## Summary of matches (no action)

- 8-byte d2cs↔d2gs header (size/type/seqno LE) — match.
- All opcodes 0x10–0x23 including dual-direction reuse — match.
- All implemented CS→GS encoder bodies (AUTHREQ/AUTHREPLY/SETGSINFO/
  SETINITINFO/SETCONFFILE) byte-for-byte — match (test-verified).
- CreateGameReq/JoinGameReq/CreateGameReply/JoinGameReply/UpdateGameInfo/
  CloseGame struct field order in wire_types — match.
- Reply/flag/difficulty constants — match.
- Client-facing JoinGameReply `addr` big-endian via bswap — match.

## Things that need real work (not blind fixes)

1. Port the d2cs↔d2gs **routing FSM** (Finding 1) — biggest gap.
2. When porting, add the CS→GS CREATEGAMEREQ/JOINGAMEREQ encoders with the
   string trailer and **ASCII** IP (Finding 2).
3. Keep the GS→CS AUTHREPLY 152-byte size guard when a decoder is added
   (Finding 5).
