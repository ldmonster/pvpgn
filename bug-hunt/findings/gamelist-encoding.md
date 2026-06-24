# Bug-hunt: game-list / game-info wire ENCODING

Subsystem: SERVER_GAMELISTREPLY (SID_GETADVLISTEX 0x09) per-game record encoding,
STARTADVEX/STARTGAME wire layout, and the bnet game-type code <-> internal
game-type mapping (game_conv.cpp).

ORIGINAL: `/home/cnupt/work/pvpgn-server`
CURRENT v3: `/home/cnupt/work/pvpgn`

Legend for classification: BUG / INTENTIONAL / UNSURE / NOT-IMPLEMENTED.

---

## Summary of findings

| # | Severity | Class | One-liner |
|---|----------|-------|-----------|
| 1 | HIGH | BUG | Missing 4-byte inter-game "spacer" (`{1,0,0,0}`) between game records in GAMELISTREPLY. |
| 2 | HIGH | BUG | `connection_fsm_inchannel` game-type switch maps bnet codes wrong (1->FFA,2->1v1,3->Coop,4->Custom vs original 2=MELEE,3=FFA,4=ONEONONE). |
| 3 | HIGH | BUG | STARTADVEX/GETADVLISTEX parser in inchannel reads game_type as LE u32 at the wrong offset; original fields are u16 at different offsets. Whole hand-rolled parser is wire-incompatible. |
| 4 | MED | UNSURE/NOT-IMPLEMENTED | `connection_fsm_inchannel` is a stand-in stub: it does NOT build a real GAMELISTREPLY, does not call gtype_to_bngtype, does not encode game records. The real codec path (codec_game.cpp) is never driven by an application builder. |
| 5 | LOW | INTENTIONAL/OK | Per-record field order + widths in codec_game.cpp match the original struct (28-byte record). Confirmed matching. |
| 6 | INFO | OK | Status-flag constants, sstatus constants, game-type request constants in game_wire_types.hpp all match the original bnet_protocol.h byte-for-byte. |
| 7 | LOW | UNSURE | gtype_to_bngtype mapping (internal->wire) has no v3 equivalent; the full correct table is reproduced below for when the application builder is implemented. |

---

## Finding 1 — Missing inter-game 4-byte spacer in GAMELISTREPLY  [HIGH / BUG]

The original prepends a 4-byte little-endian dword `{0x01,0,0,0}` (value `1`)
in front of EVERY game record EXCEPT the first one. The v3 codec writes the
records back-to-back with no separator, so any reply containing >= 2 games is
wire-corrupt from the second record onward.

ORIGINAL ref — `src/bnetd/handle_bnet.cpp:3743-3821` (`_glist_cb`):
```c
bn_int game_spacer = { 1, 0, 0, 0 };
...
if (cbdata->counter) {                                   // not the first game
    packet_append_data(cbdata->rpacket, &game_spacer, sizeof(game_spacer));
}
packet_append_data(cbdata->rpacket, &glgame, sizeof(glgame));
packet_append_string(cbdata->rpacket, game_get_name(game));
packet_append_string(cbdata->rpacket, game_get_pass(game));
packet_append_string(cbdata->rpacket, game_get_info(game));
cbdata->counter++;
```
Confirmed in the captured WCII dump embedded in `game_conv.cpp` (lines ~902-935):
each record after the first is preceded by `09 00 ...`? No — the spacer dword `01 00 00 00`
is visible: e.g. record 1 ends `...70: 00` then bytes `09 00 02 00` begin the next
gametype... and immediately before later records you see `0F 00 04 00 09 04` preceded by
the count word. The structural separator is the `game_spacer` dword the original emits.

v3 ref — `src/protocol/bnet/src/codec/codec_game.cpp:343-356` (`encode(GameListReply)`):
```cpp
for (const auto& e : m.entries) {
    w.write_le<std::uint16_t>(e.gametype);   // NO spacer before non-first entries
    w.write_le<std::uint16_t>(e.unknown1);
    w.write_le<std::uint16_t>(e.unknown3);
    w.write_be<std::uint16_t>(e.port);
    ...
}
```
Decoder `decode_game_list_reply` (lines 38-62) likewise reads records back-to-back
with no spacer consumption.

Divergence: original wire = `count(u32) sstatus(u32) [rec0] (spacer rec1) (spacer rec2)...`,
v3 wire = `count(u32) sstatus(u32) [rec0][rec1][rec2]...`. The 4-byte shift
desynchronizes every record after the first; real Blizzard clients will
misparse the list. The v3 unit test (`codec_test.cpp:476-503`) only does an
encode->decode self-round-trip, so the symmetric omission passes the test while
still being wrong against a real client.

Proposed fix: in `encode(GameListReply)` write `w.write_le<std::uint32_t>(1u)`
before each entry except the first; in `decode_game_list_reply` consume a u32
before each entry except the first. (Or model the spacer as the leading
`unknown7` dword the original struct comment references.) Add a golden-bytes
test using the WCII capture in game_conv.cpp instead of a self-round-trip.

---

## Finding 2 — Wrong game-type code mapping in inchannel switch  [HIGH / BUG]  (CONFIRMED)

CONFIRMED. Two identical wrong switches exist.

v3 ref — `src/application/connection/src/connection_fsm_inchannel.cpp`:
`on_start_game` (lines 224-230) and `on_join_game` (lines 271-278):
```cpp
const std::uint32_t raw_type = read_le32(payload, ...);
switch (raw_type) {
    case 1:  info.game_type = GameType::FreeForAll;  break;
    case 2:  info.game_type = GameType::OneOnOne;    break;
    case 3:  info.game_type = GameType::Cooperative; break;
    case 4:  info.game_type = GameType::Custom;      break;
    default: info.game_type = GameType::Melee;       break;
}
```
GameType enum (`src/domain/connection/include/domain/connection/connection_context.hpp:28-34`):
Melee=0, FreeForAll=1, OneOnOne=2, Cooperative=3, Custom=4 — an invented enum,
not the original taxonomy.

ORIGINAL ref — `src/common/bnet_protocol.h:2649-2664`
(`CLIENT_GAMELISTREQ_*`, the same codes used for the game-type field) and the
authoritative conversion in `src/bnetd/game_conv.cpp:43-166`
(`bngreqtype_to_gtype`) / `:169-295` (`bngtype_to_gtype`):
```
0x0000 ALL        0x0002 MELEE      0x0003 FFA        0x0004 ONEONONE
0x0005 CTF        0x0006 GREED      0x0007 SLAUGHTER  0x0008 SDEATH
0x0009 LADDER     0x000a MAPSET     0x000b TEAMMELEE  0x000c TEAMFFA
0x000d TEAMCTF    0x000e PGL        0x000f TOPVBOT    0x0010 IRONMAN
0x0409 DIABLO     0x0a00 LOADED
```

Divergence (the v3 switch is simply wrong):
| wire code | original meaning | v3 maps to |
|-----------|------------------|------------|
| 0 (0x0000) | ALL              | Melee (default) |
| 1          | (no such type)   | FreeForAll |
| 2 (0x0002) | MELEE            | OneOnOne   |
| 3 (0x0003) | FFA              | Cooperative |
| 4 (0x0004) | ONEONONE         | Custom     |
| 5 (0x0005) | CTF              | Melee (default) |
| ...        | GREED/SLAUGHTER/... | all collapse to Melee |

So code 2 (MELEE) becomes OneOnOne, code 3 (FFA) becomes Cooperative, code 4
(ONEONONE) becomes Custom, and every other real code (CTF, Greed, Ladder,
TeamMelee, Diablo, etc.) silently collapses to Melee. None of the rows are
correct.

Proposed fix: replace the ad-hoc enum + switch with the original
clienttag-aware tables (see Finding 7 for the full table). The mapping is
clienttag-dependent (Diablo and Diablo II reinterpret the field), so a single
flat switch cannot be correct; it must branch on the connection's clienttag
exactly like `bngtype_to_gtype`.

---

## Finding 3 — STARTADVEX/GETADVLISTEX hand-rolled parser uses wrong field widths/offsets  [HIGH / BUG]

The inchannel handlers parse the request body with hard-coded offsets and
treat the type field as a LE u32. The original STARTGAME4 (0x1cff) wire layout
uses u16 fields and the type lives at a different offset.

v3 ref — `connection_fsm_inchannel.cpp:208-253` (`on_start_game`) comment+code:
```cpp
//   [0..3]   game_state   (LE uint32 ...)
//   [4..7]   game_type    (LE uint32 ...)
//   [8..11]  unknown      (LE uint32)
//   [12..15] ladder_type  (LE uint32)
//   [16..]   game_name    (NUL-terminated)
const std::uint32_t raw_type = read_le32(payload, 4);  // reads 4 bytes at off 4
...
info.game_name = read_cstring(payload, 16);            // name at off 16
```

ORIGINAL ref — `src/common/bnet_protocol.h:2919-2933` (`t_client_startgame4`):
```c
t_bnet_header h;        // (header, not in payload)
bn_short status;        // off 0  (u16)  -- "game_state"
bn_short flag;          // off 2  (u16)
bn_int   unknown2;      // off 4  (u32)
bn_short gametype;      // off 8  (u16)  <-- the type field
bn_short option;        // off 10 (u16)  -- sub-type / option
bn_int   unknown4;      // off 12 (u32)
bn_int   unknown5;      // off 16 (u32)
/* game name  */        // off 20
/* game password */
/* game info (statstring) */
```
The correct codec (`codec_game.cpp:66-92`, `decode_startgame4_request`) already
implements this correctly: status u16, flag u16, unknown2 u32, gametype u16,
option u16, unknown4 u32, unknown5 u32, then 3 cstrings — name at offset 20.

Divergence: the inchannel parser reads `game_type` as a u32 at offset 4 (which
is actually `flag(u16)+unknown2 low u16`), and starts the game name at offset 16
(which is the middle of `unknown5`). It is wire-incompatible with STARTGAME4.
For STARTGAME1 (`:2900`-ish struct) and STARTGAME3 the layouts differ again
(u32 status, u32 unknown3, u16 gametype, u16 unknown1, ...), so there is no
single hard-coded offset set that is correct.

The `on_join_game` GETADVLISTEX parser (lines 255-301) has the same problem in
the other direction: it invents a layout (4×u32 then name at offset 20) and
treats GETADVLISTEX as if it were a join. The real GETADVLISTEX request is
`gametype(u16) unknown1(u16) unknown2(u32) unknown3(u32) max_games(u32) name`
(see `decode_game_list_req`, `codec_game.cpp:9-23`) and must produce a
GAMELISTREPLY, not a join ack.

Proposed fix: delete the hand-rolled byte parsing in
`connection_fsm_inchannel.cpp` and route through the already-correct
`codec_game.cpp` decoders (`decode_startgame4_request`,
`decode_game_list_req`), then map the decoded `gametype`/`option` via the
clienttag-aware conversion. See Finding 4.

---

## Finding 4 — inchannel is a stub: real GAMELISTREPLY is never built  [MED / NOT-IMPLEMENTED]

`on_join_game` (GETADVLISTEX) does NOT enumerate games, does NOT call
`gtype_to_bngtype`, does NOT emit any `GameListEntry`/`GameListReply`. It
assigns a fake game id, flips state to InGame, and replies with a 4-byte
`result=0` under `sid::kJoinGame` (0x22) — the wrong packet type for a
GETADVLISTEX (0x09) reply.

v3 ref — `connection_fsm_inchannel.cpp:286-300`:
```cpp
game_id_ = next_game_id_++;
state_   = ConnectionState::InGame;
ctx_.on_game_joined(game_id_, info);
std::vector<std::byte> reply;
write_le32(reply, 0u);                       // not a gamelist at all
return ctx_.send_packet(sid::kJoinGame, ...);
```

ORIGINAL ref — `handle_bnet.cpp:3826-3943` (`_client_gamelistreq`) builds a real
`SERVER_GAMELISTREPLY` (0x09ff): sets `gamecount`+`sstatus`, then either looks
up a specific game or traverses `gamelist_traverse(_glist_cb, ...)` appending one
record per game.

Divergence: the entire server->client game-list path is unimplemented in the
application layer; the codec (`codec_game.cpp`) exists but nothing drives it.
Classification NOT-IMPLEMENTED (the stub is acknowledged as placeholder in the
file header comment). Flagged because Findings 1-3 are latent until this builder
is written — when it is, it must use the codec and the correct mapping, not the
stub's invented layout.

Proposed fix: implement an application-level GETADVLISTEX handler that builds a
`GameListReply` from the live game list and sends it via
`encode(GameListReply)` (after Finding 1 is fixed), under SID 0x09.

---

## Finding 5 — Per-record field order/widths in codec MATCH original  [OK]

`encode/decode(GameListEntry)` in `codec_game.cpp` reproduces the original
`t_server_gamelistreply_game` 28-byte record exactly:

| field | original (`bnet_protocol.h:2767-2784`) | v3 (`codec_game.cpp` / `messages_game.hpp`) |
|-------|----------------------------------------|---------------------------------------------|
| gametype | bn_short (u16 LE) | write_le u16 — MATCH |
| unknown1 | bn_short (u16 LE) | write_le u16 — MATCH |
| unknown3 | bn_short (u16 LE) | write_le u16 — MATCH |
| port     | bn_short, **big-endian** | write_be u16 — MATCH |
| game_ip  | bn_int, **big-endian**   | write_be u32 — MATCH |
| unknown4 | bn_int (u32 LE) | write_le u32 — MATCH |
| unknown5 | bn_int (u32 LE) | write_le u32 — MATCH |
| status   | bn_int (u32 LE) | write_le u32 — MATCH |
| unknown6 | bn_int (u32 LE) | write_le u32 — MATCH |
| game_name / password / info | 3 × NUL-terminated strings | 3 × write_cstring — MATCH |

The port/game_ip big-endian byte order is correctly handled
(`codec_game.cpp:49-54` decode, `:347-348` encode). The only structural
divergence at the record level is the missing inter-record spacer (Finding 1).

Note: the original struct's leading `unknown7`/`deleted` commented-out fields are
NOT emitted by either side — consistent.

---

## Finding 6 — Status flags / sstatus / request-type constants MATCH  [OK]

`src/protocol/bnet/include/protocol/bnet/game_wire_types.hpp` mirrors
`bnet_protocol.h` exactly:

Game status (the per-record `status` u32) — `game_wire_types.hpp:91-94` vs
`bnet_protocol.h:2790-2794`:
```
OPEN    = 0x00000004   FULL    = 0x00000006
STARTED = 0x0000000e   DONE    = 0x0000000c
```
sstatus (reply-level) — `game_wire_types.hpp:84-89` vs `bnet_protocol.h:2760-2765`:
```
NOTFOUND=0x0  PASS=0x2  FULL=0x3  STARTED=0x4  NOSPAWNCDKEY=0x5  LOADED=0x0a00
```
"unknown" constant dwords — `game_wire_types.hpp:95-98` vs `bnet_protocol.h:2785-2796`:
```
UNKNOWN6 (latency?) = 0x0000002b
UNKNOWN1 = 0x0001   UNKNOWN3 = 0x0002
TYPE_DIABLO2_OPEN = 0x0704
```
All match. (These constants are defined but, per Finding 4, not yet consumed by
any reply builder. The original `_glist_cb` writes UNKNOWN1=0x0001,
UNKNOWN3=0x0002, UNKNOWN4=0, UNKNOWN5=0, UNKNOWN6=0x2b; a future builder must
populate the v3 `GameListEntry` with the same values.)

StartGame4 status/option bits (`game_wire_types.hpp:114-125`) also match
`bnet_protocol.h` (PRIVATE=0x01, FULL=0x02, OPEN=0x04, START=0x08,
DISCISLOSS=0x10, REPLAY=0x80, flag PRIVATE=0x0001) — these are the
open/full/started/private/replay bits the task asked about; they are present and
correct in the header, though not yet wired into the stub handlers.

---

## Finding 7 — COMPLETE correct bnet-code -> game-type mapping table  [reference]

This is the authoritative table from `src/bnetd/game_conv.cpp`
(`bngreqtype_to_gtype` :43, `bngtype_to_gtype` :169) and the constants in
`bnet_protocol.h:2649-2690`. The mapping is **clienttag-dependent** — a single
flat switch (as in the v3 stub) cannot be correct. Use this to fix Finding 2 and
to implement Finding 4.

### Warcraft II BNE  (CLIENTTAG `W2BN` / CLIENTTAG_WARCIIBNE)
| bnet code | game type |
|-----------|-----------|
| 0x0000 | game_type_all (req only) |
| 0x000f TOPVBOT | game_type_topvbot |
| 0x0002 MELEE | game_type_melee |
| 0x0003 FFA | game_type_ffa |
| 0x0004 ONEONONE | game_type_oneonone |
| 0x0009 LADDER | game_type_ladder |
| 0x0010 IRONMAN | game_type_ironman |
| 0x000a MAPSET | game_type_mapset |

### StarCraft / BroodWar / Shareware  (`STAR`/`SEXP`/`SSHR`)
| bnet code | game type |
|-----------|-----------|
| 0x0000 ALL | game_type_all |
| 0x0002 MELEE | game_type_melee |
| 0x0003 FFA | game_type_ffa |
| 0x0004 ONEONONE | game_type_oneonone |
| 0x0005 CTF | game_type_ctf |
| 0x0006 GREED | game_type_greed |
| 0x0007 SLAUGHTER | game_type_slaughter |
| 0x0008 SDEATH | game_type_sdeath |
| 0x0009 LADDER | game_type_ladder |
| 0x000a MAPSET | game_type_mapset |
| 0x000b TEAMMELEE | game_type_teammelee |
| 0x000c TEAMFFA | game_type_teamffa |
| 0x000d TEAMCTF | game_type_teamctf |
| 0x000e PGL | game_type_pgl |
| 0x000f TOPVBOT | game_type_topvbot |

### Diablo (retail/shareware)  (`DRTL`/`DSHR`)
All of 0x00..0x0d (CLIENT_GAMETYPE_DIABLO_0..DIABLO_d, char-level encoded)
-> game_type_diablo.  (bngreqtype_to_gtype only checks 0..0xd; bngtype_to_gtype
checks 0..0xc.)

### Diablo II (`D2DV`/`D2XP`)
For the request side (`bngreqtype_to_gtype`): any code -> game_type_diablo2open.
For the start/list side (`bngtype_to_gtype`):
| bnet code | game type |
|-----------|-----------|
| 0x00000008 OPEN_NORMAL | game_type_diablo2open |
| 0x00000009 OPEN_NIGHTMARE | game_type_diablo2open |
| 0x0000000a OPEN_HELL | game_type_diablo2open |
| 0x0000000c OPEN_HARDCORE_NORMAL | game_type_diablo2open |
| 0x0000000d OPEN_HARDCORE_NIGHTMARE | game_type_diablo2open |
| 0x0000000e OPEN_HARDCORE_HELL | game_type_diablo2open |
| 0x00000000 CLOSE | game_type_diablo2closed |

### Warcraft III / TFT  (`WAR3`/`W3XP`)
Always -> game_type_all (the type field is ignored; W3 carries type in the
gameinfo blob).

### Reverse: internal game_type -> bnet code  (`gtype_to_bngtype`, :298)
```
all->0x0000  melee->0x0002  ffa->0x0003  oneonone->0x0004  ctf->0x0005
greed->0x0006  slaughter->0x0007  sdeath->0x0008  ladder->0x0009
mapset->0x000a  teammelee->0x000b  teamffa->0x000c  teamctf->0x000d
pgl->0x000e  topvbot->0x000f  diablo->0x0409  diablo2open->0x0704
diablo2closed->0 (with error)  anongame->0  none/other->0xffff (error)
```

---

## Notes on what is correct / not a bug

- Record field order, widths, and port/IP big-endianness in the codec are
  correct (Finding 5).
- All status, sstatus, unknown-dword, and StartGame4 flag constants are mirrored
  correctly in `game_wire_types.hpp` (Finding 6).
- The STARTGAME1/3/4 and GAMELISTREQ/REPLY *codec* layouts in
  `codec_game.cpp` are correct; the bugs are in the application-layer stub
  (`connection_fsm_inchannel.cpp`) which bypasses the codec.
- The Diablo II difficulty / W3 mapinfo decryption logic (`game_parse_info`)
  has no v3 counterpart yet (NOT-IMPLEMENTED) — out of scope for the wire
  record encoding but noted for completeness.
