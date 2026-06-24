# Bug-hunt: d2dbs + d2cs<->d2gs (game routing / charsave / charlock / ladder)

ORIGINAL: `/home/cnupt/work/pvpgn-server`  CURRENT v3: `/home/cnupt/work/pvpgn`

## Scope / architectural context (READ FIRST)

The v3 d2dbs / d2cs / d2gs subsystem is **mostly not wired into a running server**.
Three parallel things exist, and it is critical to know which is live:

1. **Legacy bridges** (`src/app/d2dbs/src/*_bridge.cpp`, `src/app/d2cs/src/*_bridge.cpp`).
   The real PvPGN C++ (the same code as in pvpgn-server) still runs; the bridges are
   `extern "C"` shims invoked from the legacy source under `#ifdef PVPGN_V3_*_INTEGRATION`.
   - Most are **observation-only** (log + `return 0` → legacy path continues): e.g.
     `dbspacket_bridge.cpp`, `d2ladder_bridge.cpp`, `charlock_bridge.cpp`,
     `handle_d2gs_packet_bridge.cpp`, `send_outbound_obs_bridges.cpp`.
   - A handful **actually encode bytes** and send them (these are the ones that can have
     wire bugs): `send_handle_d2gs_bridges.cpp`, `send_joingamereply_bridge.cpp`,
     `send_creategamereply_bridge.cpp`, `send_simple_replies.cpp`, etc.

2. **v3 protocol codecs** (`src/protocol/d2dbs`, `src/protocol/d2gs`, `src/protocol/d2cs`).
   Pure encode/decode. Two of them have a *second* parallel implementation
   (`codec.cpp` vs `fsm.cpp` for d2dbs) that disagree with each other — see F1.

3. **v3 domain + session handler** (`src/domain/d2dbs`, `src/app/d2dbs/src/d2dbs_session_handler.cpp`).
   This uses the **FSM** (not the codec) and the in-memory repos. Its egress port
   (`ID2DBSSessionEgress`) has **no production implementation** — only a unit-test fake.
   So this whole "clean" d2dbs path is dead code w.r.t. the live server today.

Implication for severity: several findings below are real divergences in *implemented*
v3 code, but because the live behaviour is still the legacy C++, the user-visible blast
radius is gated on when the v3 path is switched on. They are flagged accordingly.

---

## F1. d2dbs codec `SaveDataRequest` / `GetDataRequest` drop the RealmName field — wire layout broken

- **Severity:** HIGH (wire-incompatible) — but in a non-live codec path (see scope).
- **Classification:** BUG
- **Original ref:** `src/d2dbs/dbspacket.cpp:294-333` (`dbs_packet_savedata`) and
  `:388-431` (`dbs_packet_getdata`). Both read **three** cstrings after the fixed fields:
  ```
  readpos += sizeof(*savecom);
  strncpy(AccountName, readpos, ...); readpos += strlen+1;
  strncpy(CharName,    readpos, ...); readpos += strlen+1;
  strncpy(RealmName,   readpos, ...); readpos += strlen+1;   // <-- present
  ```
  i.e. wire = `[hdr][datatype][datalen][AccountName\0][CharName\0][RealmName\0][data]`.
- **v3 ref:** `src/protocol/d2dbs/src/codec.cpp:48-67` (`dec_save_data_req`) and
  `:83-95` (`dec_get_data_req`). Each reads only AccountName + CharName, then the blob.
  `encode(SaveDataRequest)` (`:268-281`) and `encode(GetDataRequest)` (`:293-303`) also
  write only account + charname. The structs in `codec.hpp:85-110` have **no `realmname`
  field** (compare `UpdateLadderRequest`/`CharLockRequest`, which *do* have `realmname`).
- **Divergence:** v3 codec is missing the RealmName cstring for SAVE_DATA(0x30) and
  GET_DATA(0x31). Decoding a real D2GS packet would read RealmName's bytes as the start of
  the save blob (and the realm is lost); encoding produces a packet a real D2GS/D2DBS
  rejects (`readpos + datalen != ReadBuf + size` size check fails in the original).
- **Note — the FSM disagrees and is correct:** `src/protocol/d2dbs/src/fsm.cpp:112-220`
  (`handle_save_data`, `handle_get_data`) *does* read `realm_name` as the third cstring,
  matching the original, and `domain::d2dbs::CharacterSaveData` carries `realm_name`. So
  the two v3 d2dbs implementations are internally inconsistent; the codec is the wrong one.
- **Proposed fix:** Add `std::string realmname;` to `SaveDataRequest` and `GetDataRequest`
  in `codec.hpp`; read/write it as the third cstring in `codec.cpp` (between charname and
  the data blob for save; after charname for get). Update `kWireBytes*`/tests accordingly.

## F2. v3 GET_DATA load path performs NO character lock and NO dupe-check

- **Severity:** HIGH (dupe-prevention) — gated on v3 session path going live.
- **Classification:** BUG
- **Original ref:** `src/d2dbs/dbspacket.cpp:440-498`. For `D2GS_DATA_CHARSAVE`, GET_DATA:
  1. `cl_query_charlock_status(...)` → if locked elsewhere, reply `D2DBS_GET_DATA_CHARLOCKED`.
  2. else `cl_lock_char(CharName, RealmName, conn->serverid)` — **acquire the lock as a side
     effect of loading** (this is the in-game dupe lock).
  3. if the charinfo/charsave read fails, `cl_unlock_char(...)` to roll back.
- **v3 ref:** `src/app/d2dbs/src/d2dbs_session_handler.cpp:86-98` (`handle_char_load`) just
  calls `CharacterLoadUseCase::execute` → `repo_.load(account, char_name)`
  (`domain/.../use_cases.hpp:71-74`, `in_memory_repositories.hpp:48-53`). No lock query, no
  lock acquire, no rollback. The egress is `send_char_load_result(bool, data*)`
  (`d2dbs_session_egress.hpp:69-72`) — a 2-state success/fail with **no CHARLOCKED tri-state**,
  so even the `D2DBS_GET_DATA_CHARLOCKED` result code (`codec.hpp:52`) can never be produced.
- **Divergence:** The core anti-dupe mechanism (a char is locked while loaded into a game,
  preventing a second game server from loading the same char) is absent on the load path.
  Locking only happens on an explicit CHAR_LOCK(0x33) packet in v3 — but the original *also*
  locks implicitly during GET_DATA, which is what stops the load race.
- **Proposed fix:** In `handle_char_load`, before loading for CHAR_SAVE: query lock state;
  if locked → emit a CHARLOCKED reply; else acquire the lock, load, and on load failure
  release it. Extend the egress to carry the tri-state result (Success/Failed/CharLocked)
  and `charcreatetime` + `allowladder` (see F3).

## F3. v3 GET_DATA reply never sets `charcreatetime` / `allowladder`

- **Severity:** MEDIUM — gated on v3 path going live.
- **Classification:** BUG / NOT-IMPLEMENTED
- **Original ref:** `src/d2dbs/dbspacket.cpp:484-497`. On success it reads the charinfo
  header `create_time`, sets `getret->charcreatetime`, and computes
  `allowladder = (create_time >= prefs_get_ladderinit_time()) ? 1 : 0`.
- **v3 ref:** `src/protocol/d2dbs/src/fsm.cpp:356-379` (`make_get_data_reply`) *has*
  `charcreatetime`/`allowladder` params, but the only caller path
  (`d2dbs_session_handler.cpp:86-98` → `send_char_load_result(bool, data*)`) cannot supply
  them; the egress signature drops both. So replies will always carry 0/0.
- **Divergence:** Ladder eligibility (`allowladder`) and char creation time are lost; a real
  D2GS would treat every loaded char as ladder-ineligible.
- **Proposed fix:** Plumb `charcreatetime` (from the .d2s/charinfo header) and the
  `ladderinit_time` comparison through the use case + egress into the reply.

## F4. v3 char-save use case skips the D2 charsave checksum validation

- **Severity:** MEDIUM — gated on v3 path going live.
- **Classification:** BUG
- **Original ref:** `src/d2dbs/dbspacket.cpp:86-94` (`dbs_packet_savedata_charsave`):
  reads the checksum at `D2CHARSAVE_CHECKSUM_OFFSET`, recomputes via
  `d2charsave_checksum(...)`, and **discards the save** if they mismatch (anti-corruption /
  anti-tamper).
- **v3 ref:** `d2dbs_session_handler.cpp:69-84` (`handle_char_save`) → `CharacterSaveUseCase`
  → `repo_.save(data)` (`in_memory_repositories.hpp:55-58`). No checksum check; any blob is
  written verbatim.
- **Divergence:** Corrupt/forged charsaves are persisted unconditionally.
- **Proposed fix:** Port the `d2charsave_checksum` validation into the save use case (or a
  domain validator) and reject on mismatch, mirroring the original return-0 behaviour.

## F5. v3 CHAR_LOCK(0x33) handler emits a reply; original sends none

- **Severity:** LOW — gated on v3 path going live.
- **Classification:** BUG (extra/unexpected packet)
- **Original ref:** `src/d2dbs/dbspacket.cpp:568-624` (`dbs_packet_charlock`): locks/unlocks
  and `return 1` — it **never writes to `WriteBuf`**, i.e. no reply packet to the D2GS.
- **v3 ref:** `d2dbs_session_handler.cpp:122-137` (`handle_char_lock`) calls
  `egress_.send_char_login_result(ok)` / `send_char_logout_result(ok)`.
- **Divergence:** If the egress encodes any bytes for these, v3 injects a reply the legacy
  protocol never sends; a real D2GS may treat it as an unknown/duplicate packet. (Today the
  egress is a test fake, so harmless until wired.)
- **Proposed fix:** Make the lock/unlock egress calls no-ops on the wire (lock result is only
  surfaced indirectly via the next GET_DATA CHARLOCKED), or confirm a reply is truly desired.

## F6. d2cs<->d2gs game-routing packets (CREATE/JOIN/CLOSE/UPDATEGAMEINFO) not implemented in v3 codecs

- **Severity:** INFO (no regression; legacy handles it)
- **Classification:** NOT-IMPLEMENTED
- **Original ref:** `src/common/d2cs_d2gs_protocol.h:138-209`
  (`CREATEGAMEREQ 0x20`, `CREATEGAMEREPLY 0x20`, `JOINGAMEREQ 0x21`, `JOINGAMEREPLY 0x21`,
  `UPDATEGAMEINFO 0x22`, `CLOSEGAME 0x23`); request/handling in
  `src/d2cs/handle_d2gs.cpp` / `s2s.cpp` / `serverqueue.cpp`.
- **v3 ref:** The wire structs exist in `src/protocol/d2gs/include/protocol/d2gs/wire_types.hpp`
  (`CreateGameReq:159`, `CreateGameReply:169`, `JoinGameReq:177`, `JoinGameReply:185`,
  `UpdateGameInfo:193`, `CloseGame:203`) with correct fixed-field layouts and opcodes, BUT
  the d2gs codec `src/protocol/d2gs/src/codec.cpp` implements encode/decode only for
  AuthReq/AuthReply/SetGsInfo/Echo/Control. Create/Join/Close/UpdateGameInfo have **no
  encode/decode**. The d2cs outbound stubs `send_outbound_obs_bridges.cpp:20-34`
  (`obs_creategamereq_d2gs`, `obs_joingamereq_d2gs`, `obs_control_d2gs`) just `return 0`.
- **Divergence:** Game create/join/close routing between d2cs and d2gs is entirely legacy.
- **Proposed fix:** None required now; when porting, add codec encode/decode for 0x20–0x23.
  Watch the trailing cstrings the structs intentionally omit: CREATEGAMEREQ also carries
  `gamename\0 gamepass\0 gamedesc\0 acct\0 char\0 ip\0`; JOINGAMEREQ carries `char\0 acct\0 ip\0`;
  UPDATEGAMEINFO carries `charname\0` (see `d2cs_d2gs_protocol.h:146-198`).

## F7. v3 binary ladder file format (`t_d2ladderfile_*`) not implemented

- **Severity:** INFO (no regression; legacy maintains the file)
- **Classification:** NOT-IMPLEMENTED
- **Original ref:** `src/common/d2cs_d2dbs_ladder.h:28-48` defines the on-disk format:
  header `{bn_int maxtype; bn_int checksum}`, then `D2LADDER_MAXTYPE(35)` index entries
  `{bn_int type; bn_int offset; bn_int number}`, then per-type arrays of
  `{bn_int experience; bn_short status; bn_byte level; bn_byte chclass; char charname[MAX_CHARNAME_LEN]}`.
  Sizes/limits in `src/d2dbs/d2ladder.h:57-59` (`MAXNUM 200`, `OVERALL_MAXNUM 1000`,
  `MAXTYPE 35`); read/write loops in `src/d2dbs/d2ladder.cpp`.
- **v3 ref:** `src/domain/d2dbs` ladder is an in-memory `std::map<charname, LadderUpdateEntry>`
  (`in_memory_repositories.hpp:135-171`); `LadderUpdateEntry` (`types.hpp:58-65`) holds
  64-bit experience, level, class, flags — no per-type tables, no offsets, no checksum, no
  file I/O. `d2ladder_bridge.cpp` is observation-only. No code reproduces the binary file.
- **Divergence:** The persisted ladder binary is legacy-only in v3.
- **Note (field mapping, when ported):** original `dbs_packet_updateladder`
  (`dbspacket.cpp:558-562`) stores **only `charexplow`** as experience (high word ignored);
  v3 `handle_char_ladder` (`d2dbs_session_handler.cpp:103-106`) combines hi<<32|lo into 64
  bits. Harmless until the on-disk `bn_int experience` (32-bit) is written — at that point
  the v3 64-bit value must be truncated to low 32 bits to match the file format.

---

## Things that MATCH (verified correct)

- **d2dbs 8-byte header** `[size:u16 LE][type:u16 LE][seqno:u32 LE]`: original
  `t_d2dbs_d2gs_header` (`dbspacket.h:31-35`) == v3 `D2dbsHeader` (`codec.hpp:58-64`) ==
  FSM header parse (`fsm.cpp:38-62`). No per-packet checksum in either (correct — the only
  checksum is the D2-internal one inside the charsave blob).
- **d2dbs opcodes:** SAVE 0x30, GET 0x31, UPDATE_LADDER 0x32, CHAR_LOCK 0x33, ECHO 0x34;
  CONNECT class 0x65; datatype CHARSAVE 0x01 / PORTRAIT 0x02; result codes
  SUCCESS 0 / FAILED 1 / CHARLOCKED 2. All match (`dbspacket.h:40-116` vs `codec.hpp:37-56`
  and `fsm.hpp:36-53`).
- **SAVE_DATA reply** `[hdr][result:u32][datatype:u16][CharName\0]` — original
  `dbspacket.cpp:365-378` == v3 codec `encode(SaveDataReply)` (`codec.cpp:283-291`) ==
  FSM `make_save_data_reply` (`fsm.cpp:339-354`).
- **GET_DATA reply** `[hdr][result:u32][charcreatetime:u32][allowladder:u32][datatype:u16][datalen:u16][CharName\0][data]`
  — original `dbspacket.cpp:511-522` == codec `encode(GetDataReply)` (`codec.cpp:305-319`) ==
  FSM `make_get_data_reply` (`fsm.cpp:356-379`). (Population of the fields is F3.)
- **UPDATE_LADDER request** `[hdr][charlevel:u32][charexplow:u32][charexphigh:u32][charclass:u16][charstatus:u16][CharName\0][RealmName\0]`
  — original `t_d2gs_d2dbs_update_ladder` (`dbspacket.h:89-98`) == codec
  `dec_update_ladder` (`codec.cpp:122-142`) == FSM `handle_update_ladder` (`fsm.cpp:222-270`).
- **CHAR_LOCK request** `[hdr][lockstatus:u32][AccountName\0][CharName\0][RealmName\0]`,
  lockstatus!=0 ⇒ lock — original `dbspacket.h:100-106`/`dbspacket.cpp:568-622` matches the
  FSM `handle_char_lock` (`fsm.cpp:272-317`). (The codec `dec_char_lock` at `codec.cpp:144-156`
  omits AccountName — same RealmName/account class of issue as F1, but here it's the *account*
  that's dropped; the FSM is the correct one and is what's used.)
- **ECHO keepalive:** D2DBS→D2GS echorequest and D2GS→D2DBS echoreply both type 0x34,
  header-only — original `dbspacket.cpp:771-794` (`dbs_keepalive`) and `:382-386`
  (`dbs_packet_echoreply`) match v3 (`codec.cpp:184-185,260-266`, `fsm.cpp:319-333,381-387`).
  Note original `dbs_keepalive` leaves `seqno=0` (FIXME in original) — v3 keepalive seqno
  handling is equivalent.
- **CONNECT handshake:** single byte 0x65 read before framed protocol — original
  `dbspacket.cpp:638-660` == v3 `decode_connect_handshake` (`codec.cpp:240-253`).
- **d2cs<->d2gs internal header & opcodes:** `t_d2cs_d2gs_header` (`d2cs_d2gs_protocol.h:37-42`,
  8 bytes) == v3 `d2gs::wire::Header` (`wire_types.hpp:21-29`). Opcodes 0x10–0x23 all match
  (`wire_types.hpp:33-48`).
- **d2cs<->d2gs AUTHREQ** `[hdr][sessionnum:u32][signlen:u32][realmname\0]` — original
  build at `handle_d2gs.cpp:489-495` (sets signlen, appends realmname only, no sign data) ==
  v3 `send_authreq_d2gs` bridge (`send_handle_d2gs_bridges.cpp:38-54`) and codec
  `encode(DownAuthReq)` (`d2gs/codec.cpp:182-190`).
- **d2cs<->d2gs SETINITINFO / SETCONFFILE / SETGSINFO / AUTHREPLY / CONTROL** field orders
  match between `handle_d2gs.cpp` (lines 102-110, 139-144, 222-223, 252-254) and the v3
  `send_handle_d2gs_bridges.cpp` + `d2gs/codec.cpp`.
- **AUTHREPLY-from-d2gs** `[hdr][version:u32][checksum:u32][randnum:u32][signlen:u32][sign:128]`
  — original `t_d2gs_d2cs_authreply` (`d2cs_d2gs_protocol.h:60-68`) == v3
  `decode_d2gs_to_d2cs`/`UpAuthReply` (`d2gs/codec.cpp:132-143`, `wire_types.hpp:96-105`).
- **Client JOINGAMEREPLY** `[hdr][seqno:u16][gameid:u16][u1:u16][addr:u32][token:u32][reply:u32]`
  — original `t_d2cs_client_joingamereply` (`d2cs_protocol.h:127-137`) == v3 `JoinGameReply`
  (`d2cs/wire_types.hpp:197-209`) and the `send_joingamereply_bridge.cpp`. Crucially the
  bridge's `bswap32(addr)` (`send_joingamereply_bridge.cpp:16-39`) correctly reproduces the
  original's use of **`bn_int_nset`** (big-endian) for `addr` at `handle_d2gs.cpp:417`,
  while every other field uses LE `bn_int_set`. Good, subtle, and correct.
- **Client CREATEGAMEREPLY** `[hdr][seqno:u16][gameid:u16][u1:u16][reply:u32]` —
  original `d2cs_protocol.h:101-109` == v3 `CreateGameReply` (`d2cs/wire_types.hpp:176-186`).

---

## Summary of findings

| ID | Sev   | Class            | One-liner |
|----|-------|------------------|-----------|
| F1 | HIGH  | BUG              | d2dbs **codec** drops RealmName on SAVE/GET (FSM is correct; codec is inconsistent) |
| F2 | HIGH  | BUG              | v3 GET_DATA load does no charlock / dupe-check / CHARLOCKED reply |
| F3 | MED   | BUG/NOT-IMPL     | v3 GET_DATA reply never fills charcreatetime/allowladder |
| F4 | MED   | BUG              | v3 char-save skips D2 charsave checksum validation |
| F5 | LOW   | BUG              | v3 CHAR_LOCK handler emits a reply the original never sends |
| F6 | INFO  | NOT-IMPLEMENTED  | d2cs<->d2gs CREATE/JOIN/CLOSE/UPDATEGAMEINFO routing absent from v3 codecs |
| F7 | INFO  | NOT-IMPLEMENTED  | binary ladder file format not reproduced in v3 |

All findings except F1 are currently inert because the v3 d2dbs session path (FSM + domain +
egress) has no production egress impl and the live server still runs the legacy C++. F1 is a
real, self-contained correctness bug inside the v3 codec library that whatever consumes
`protocol::d2dbs::codec` (tests, future wiring) will hit.
