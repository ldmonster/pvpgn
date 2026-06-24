# Gameplay / Game-list subsystem — ORIGINAL vs v3 bug hunt

Scope: hosted games (create/join/start/close), game type/flags/status, GETADVLISTEX/STARTADVEX
handling and the raw game-type → enum mapping.

ORIGINAL refs: `pvpgn-server/src/bnetd/game.{h,cpp}`, `game_conv.cpp`,
`handle_bnet.cpp`, `common/bnet_protocol.h`, `common/field_sizes.h`.
v3 refs: `pvpgn/src/domain/gameplay/include/domain/gameplay/game.hpp`,
`domain/connection/include/domain/connection/connection_context.hpp`,
`application/connection/src/connection_fsm_inchannel.cpp`.

---

## FINDING 1 — Game-type wire-code mapping is completely wrong (CRITICAL BUG)

**Severity:** Critical
**Classification:** BUG

### Original
The bnet wire game-type codes (`CLIENT_GAMELISTREQ_*`) are defined in
`pvpgn-server/src/common/bnet_protocol.h:2649-2665`:

```
#define CLIENT_GAMELISTREQ_ALL       0x0000
#define CLIENT_GAMELISTREQ_MELEE     0x0002
#define CLIENT_GAMELISTREQ_FFA       0x0003
#define CLIENT_GAMELISTREQ_ONEONONE  0x0004
#define CLIENT_GAMELISTREQ_CTF       0x0005
#define CLIENT_GAMELISTREQ_GREED     0x0006
#define CLIENT_GAMELISTREQ_SLAUGHTER 0x0007
#define CLIENT_GAMELISTREQ_SDEATH    0x0008
#define CLIENT_GAMELISTREQ_LADDER    0x0009
#define CLIENT_GAMELISTREQ_MAPSET    0x000a   (10)
#define CLIENT_GAMELISTREQ_TEAMMELEE 0x000b   (11)
#define CLIENT_GAMELISTREQ_TEAMFFA   0x000c   (12)
#define CLIENT_GAMELISTREQ_TEAMCTF   0x000d   (13)
#define CLIENT_GAMELISTREQ_PGL       0x000e   (14)
#define CLIENT_GAMELISTREQ_TOPVBOT   0x000f   (15)
#define CLIENT_GAMELISTREQ_IRONMAN   0x0010   (16)
```

Mapping done by `bngtype_to_gtype()` / `bngreqtype_to_gtype()`
(`game_conv.cpp:169-295`, `43-166`), keyed on clienttag. Note: there is **no code 1**;
`0x0000`=ALL, `0x0001` is unused, `0x0002`=MELEE, `0x0003`=FFA, `0x0004`=ONEONONE,
`0x0005`=CTF.

### v3
`connection_fsm_inchannel.cpp:224-230` (STARTADVEX) and `272-278` (GETADVLISTEX):

```cpp
switch (raw_type) {
    case 1:  info.game_type = GameType::FreeForAll;  break;
    case 2:  info.game_type = GameType::OneOnOne;    break;
    case 3:  info.game_type = GameType::Cooperative; break;
    case 4:  info.game_type = GameType::Custom;      break;
    default: info.game_type = GameType::Melee;       break;
}
```

### Divergence
The numeric codes do not match the protocol at all:

| raw code | original meaning      | v3 maps to            |
|----------|-----------------------|-----------------------|
| 0        | ALL                   | Melee  (default)      |
| 1        | (unused)              | FreeForAll            |
| 2        | **MELEE**             | OneOnOne   (WRONG)    |
| 3        | **FFA**               | Cooperative (WRONG)   |
| 4        | **ONEONONE**          | Custom     (WRONG)    |
| 5        | CTF                   | Melee  (default)      |

Every populated case is wrong. A Starcraft/WarCraft II MELEE game (code 2) becomes
OneOnOne; FFA (3) becomes "Cooperative"; 1v1 (4) becomes "Custom". The actual MELEE
default lands only on codes the client never sends as the primary type. The mapping
also ignores `clienttag`, which the original requires (same numeric code means
different things for D2 vs SC vs WC2). The v3 `GameType` enum
(`connection_context.hpp:28-34`) is itself a fabricated 5-value set
{Melee, FreeForAll, OneOnOne, Cooperative, Custom} that does not correspond to the
original 20-value `t_game_type` (`game.h:48-71`: none/all/topvbot/melee/ffa/oneonone/
ctf/greed/slaughter/sdeath/ladder/ironman/mapset/teammelee/teamffa/teamctf/pgl/diablo/
diablo2open/diablo2closed/anongame). "Cooperative" and "Custom" do not exist in the
original; CTF/Greed/Slaughter/SuddenDeath/Ladder/Ironman/Mapset/Team*/PGL/TopVBot are
all lost.

### Proposed fix
Replace the hand-rolled switch with a clienttag-aware mapping that mirrors
`bngtype_to_gtype` (and the request variant `bngreqtype_to_gtype` for GETADVLISTEX,
which differs slightly — e.g. WC2/D2 ALL handling). Expand the v3 `GameType` enum to
cover the real `t_game_type` set, or store the raw 16-bit `t_game_type` value. At
minimum, fix the numeric cases: 0→All, 2→Melee, 3→FFA, 4→OneOnOne, 5→CTF, etc.

---

## FINDING 2 — Game-type field read as 32-bit; original is 16-bit (BUG)

**Severity:** Medium
**Classification:** BUG

### Original
`handle_bnet.cpp:3855`: `bngtype = bn_short_get(packet->u.client_gamelistreq.gametype);`
The game type on the wire is a **16-bit** field (`unsigned short bngtype`,
`game_conv.cpp:169`).

### v3
`connection_fsm_inchannel.cpp:271`: `const std::uint32_t raw_type = read_le32(payload, 0);`
(also `:223` for STARTADVEX). v3 reads a 32-bit value.

### Divergence
For STARTADVEX the game-state/type words are 32-bit in that packet, so reading 32 there
is arguably fine, but for the GETADVLISTEX/GAMELISTREQ path the original treats the
gametype as a short. More importantly, combined with Finding 1 the offsets/width are not
matched to the original packet structs (`t_client_gamelistreq`, `t_client_startgame*`).
This should be verified against the exact original packet layout rather than the
hand-written offset comments in the v3 file.

### Proposed fix
Confirm field widths/offsets against the original packet structs in
`common/packet.h` and read the correct width.

---

## FINDING 3 — GETADVLISTEX is a list QUERY, not a join; v3 creates a game + enters InGame (HIGH BUG)

**Severity:** High
**Classification:** BUG

### Original
`SID_GETADVLISTEX` / `CLIENT_GAMELISTREQ` is a **read-only game-list query**.
`handle_bnet.cpp:_client_gamelistreq` (3840-3960+) parses the filter, then either
finds a specific game (`gamelist_find_game`) or enumerates games and emits a
`SERVER_GAMELISTREPLY` listing them. It does **not** create a game, allocate a game id,
or change the connection's state. Joining a game is a separate later flow (the client
picks a game and the host/UDP path adds the player).

### v3
`connection_fsm_inchannel.cpp:255-301` `on_join_game()` (mapped to GETADVLISTEX 0x09):

```cpp
game_id_ = next_game_id_++;
state_   = ConnectionState::InGame;
ctx_.on_game_joined(game_id_, info);
```

It allocates a brand-new game id, transitions the connection to `InGame`, and fires
`on_game_joined` for what is actually a directory query.

### Divergence
Semantically wrong lifecycle: a player browsing the game list is moved into "in game"
state and a phantom game is created on every list request. The reply also just writes
`result=0` instead of an enumerated `SERVER_GAMELISTREPLY` with game entries
(count, status flags, per-game records). Clients expecting a game list get an empty/
malformed reply and the server's connection state machine desyncs.

### Proposed fix
GETADVLISTEX must be handled as a query: enumerate matching games and build a
GAMELISTREPLY; do not allocate a game id or transition to InGame. A real "join" should
be a distinct event with its own SID, not GETADVLISTEX.

---

## FINDING 4 — STARTADVEX game_state (private/public/protected) parsed then discarded (BUG)

**Severity:** Medium
**Classification:** BUG

### Original
The STARTADVEX game state field maps to `game_status` / private flag. `game.h:177-180`
defines `game_flag_none` / `game_flag_private`; password presence and the state word
mark a game private/protected. Status word values:
`bnet_protocol.h:2816-2820`: `STATUSMASK 0x0f`, `STATUS_OPEN 0x04`, `STATUS_FULL 0x06`,
`STATUS_STARTED 0x0e`, `STATUS_DONE 0x0c` — used in `handle_bnet.cpp:4060-4074` to set
`game_status_open/full/started/done`.

### v3
`connection_fsm_inchannel.cpp:213-216` documents:
`[0..3] game_state (LE uint32: 0=private, 1=public, 2=protected)`
but the value at offset 0 is never read in `on_start_game()` — only `raw_type` at
offset 4 is consumed. `GameInfo` (`connection_context.hpp:40-46`) has no
private/protected/status field, so the public/private distinction is dropped entirely.

### Divergence
Private games are not flagged as private; the `game_flag_private` concept is missing.
The 0/1/2 meaning in the comment also disagrees with the original status-mask encoding
(0x04/0x06/0x0e/0x0c), so even if read it would be misinterpreted.

### Proposed fix
Read the game_state word, decode it per the original status mask, and carry a
private/status flag on `GameInfo` / the gameplay aggregate.

---

## FINDING 5 — No game name / password length limits (BUG)

**Severity:** Low/Medium
**Classification:** BUG

### Original
`common/field_sizes.h:41-42`: `MAX_GAMENAME_LEN = 32`, `MAX_GAMEPASS_LEN = 32`.
`handle_bnet.cpp:3845-3853` rejects packets whose gamename/gamepass exceed these
(`packet_get_str_const(..., MAX_GAMENAME_LEN)` returns NULL → `-1`).

### v3
`connection_fsm_inchannel.cpp:233-237, 281-285` read `game_name`, `password`,
`game_stats` via `read_cstring` with no length cap and no rejection of over-long
fields.

### Divergence
Over-long names/passwords are accepted instead of rejected. Minor protocol parity /
robustness gap.

### Proposed fix
Enforce the 32/32 limits (and the gamestats/info limit) and reject over-long fields.

---

## FINDING 6 — max_players default differs (UNSURE / minor)

**Severity:** Low
**Classification:** UNSURE

### Original
`game.cpp:451`: a freshly created `t_game` sets `game->maxplayers = 0` (the real cap is
filled in later from the game type / map / client). Default is effectively "unknown/0
until set."

### v3
`game.hpp:43`: `std::uint8_t max_players = 8;` and `host()` rejects `0` or `>16`
(`game.hpp:58-61`); `is_full()` uses this cap (`:89-91`).

### Divergence
v3 invents a fixed default of 8 and a hard 1..16 range. The original derives the cap
from the game type (e.g. oneonone=2, topvbot variants 1..7, teammelee 2..4) rather than
a flat 8, and never rejects on a 1..16 range at creation. The v3 default and clamp are
plausible but are not a faithful port of the type-derived caps; mark UNSURE pending the
intended design.

### Proposed fix
If parity is intended, derive max_players from the resolved game type (see
`game.h:93-128` `t_game_option` variants and the per-type player counts) rather than a
flat 8.

---

## Things that MATCH / are reasonable

- **Lifecycle / host-can-start.** v3 `Game::start()` (`game.hpp:122-128`) requires
  `by == host_` and `state_ == Open`, mirroring the original auth/lifecycle intent
  (only the game owner advances status; original gates start via
  `account_get_auth_createladdergame/createnormalgame`, `handle_bnet.cpp:4080-4084`).
  The auth-class gating itself is not reproduced in the FSM but the host-only,
  Open→InProgress transition is consistent.
- **Host migration on leave.** v3 `Game::leave()` (`game.hpp:107-120`) migrates host to
  the next player when the host leaves and players remain — a reasonable, explicit
  improvement over the original (original deletes/cleans empty games via
  `MAX_GAME_EMPTY_TIME`, `game.cpp:2305`).
- **Game states Open→InProgress→Reporting→Finalized** (`game.hpp:33-38`) are a clean
  re-modeling; they don't 1:1 match the original `started/full/open/loaded/done`
  status enum (`game.h:73-80`) but the original status set is a wire/status concept,
  whereas v3's is an aggregate lifecycle — different layers, not a direct contradiction.
  (The wire-status decoding gap is captured in Finding 4.)
- **is_full** logic (`game.hpp:89-91`) is correct given a correct max_players.

---

## Summary of bugs (by severity)

1. **CRITICAL** — game-type raw-code → enum switch is wrong for every case and ignores
   clienttag; whole `GameType` enum is a fabricated 5-value set that drops ~15 real
   game types (Finding 1).
2. **HIGH** — GETADVLISTEX (a list query) is handled as a join: creates a phantom game,
   advances connection to InGame, returns no game list (Finding 3).
3. **MEDIUM** — STARTADVEX game_state/private bit parsed-but-discarded; no private flag
   on GameInfo (Finding 4).
4. **MEDIUM** — game type read as 32-bit where original is 16-bit / unverified offsets
   (Finding 2).
5. **LOW/MED** — no gamename/gamepass length enforcement (32/32) (Finding 5).
6. **LOW/UNSURE** — max_players default 8 + 1..16 clamp vs original type-derived cap
   (Finding 6).
