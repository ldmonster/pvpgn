# R280 — Evaluate Variant Sub-Codecs (STAR / D2DV / WAR3)

**Status:** COMPLETE — Evaluation only; no source code modified.  
**Decision:** **DEFER** to a later phase (Phase F / Plan 06 extension, post-Phase E).

---

## 1. Investigation Findings

### 1.1 `src/v3/protocol/bnet/include/protocol/bnet/codec.hpp`

- Exposes two top-level decode functions:
  - [`decode_client(const Packet&)`](src/v3/protocol/bnet/include/protocol/bnet/codec.hpp:21)
  - [`decode_server(const Packet&)`](src/v3/protocol/bnet/include/protocol/bnet/codec.hpp:25)
- Both return a **single flat variant** (`ClientMessage` / `ServerMessage`).
- There is **no per-game-tag dispatch** at the codec level. The codec is
  product-agnostic: it decodes every SID it knows about regardless of which
  game client sent it.
- No `variants/` subdirectory exists under `src/v3/protocol/bnet/`.

### 1.2 `src/v3/protocol/bnet/include/protocol/bnet/messages.hpp`

- [`ClientMessage`](src/v3/protocol/bnet/include/protocol/bnet/messages.hpp:1760)
  and [`ServerMessage`](src/v3/protocol/bnet/include/protocol/bnet/messages.hpp:1849)
  are **unified flat `std::variant`** types — 60+ arms each.
- The `game_id` / `clienttag` field is carried **inside** individual message
  structs (e.g. [`AuthInfo::game_id`](src/v3/protocol/bnet/include/protocol/bnet/messages.hpp:151),
  [`LadderSearchRequest::client_tag`](src/v3/protocol/bnet/include/protocol/bnet/messages.hpp:319),
  [`FriendEntry::client_tag`](src/v3/protocol/bnet/include/protocol/bnet/messages.hpp:384))
  as a plain `uint32_t` field — not as a codec-level discriminator.
- **No STAR/D2DV/WAR3-specific message structs exist.** All game variants share
  the same wire structs; the differences are in *field interpretation* (e.g.
  statstring format), not in packet layout.
- The only WAR3-specific codec path is the `WarcraftGeneralRequest` /
  `WarcraftGeneralReply` pair (SID 0x44), which is already present and handled
  via the [`anongame.hpp`](src/v3/protocol/bnet/include/protocol/bnet/anongame.hpp)
  sub-parser.

### 1.3 `plans/06-protocol-and-codecs.md` — Section 6 "Versioning & feature gates"

Plan 06 §6 explicitly describes the future variant sub-codec design:

> Each variant gets a sub-codec (`protocol/bnet/variants/{star,d2dv,d2xp,war3}.h`).  
> The pump selects the variant after `CLIENT_AUTH_INFO` parses, then routes all
> subsequent frames through the variant codec.  
> Common fields stay in `protocol/bnet/common.h`.

This is described as **future state**, not a current task. The concrete task
list in §7 (R250–R261) does **not** include a task for variant sub-codecs.

### 1.4 Existing game-tag-specific codec files in `src/v3/protocol/`

A full recursive listing of [`src/v3/protocol/`](src/v3/protocol/) reveals:

| Subdirectory | Purpose |
|---|---|
| `bnet/` | Main BNet SID codec (unified, no per-game split) |
| `common/` | `DecodeError`, `next_frame`, `Packet`, `Reader`, `Writer` |
| `d2cs/`, `d2dbs/`, `d2gs/`, `d2save/` | Diablo II server-to-server protocols |
| `file/`, `irc/`, `telnet/`, `udp/` | Other protocol codecs |
| `wol/`, `wolgameres/` | Westwood Online codecs |

**No `variants/` directory exists.** No files named `star.hpp`, `d2dv.hpp`,
`war3.hpp`, or similar exist anywhere under `src/v3/protocol/`.

### 1.5 Legacy `src/bnetd/` / `src/v3/integration/` — Scope of variant handling

The legacy code in
[`src/v3/integration/legacy_bnetd/src/handle_bnet_link.cpp`](src/v3/integration/legacy_bnetd/src/handle_bnet_link.cpp)
contains **extensive** per-`clienttag` branching. Key examples:

| Location | Variant-specific logic |
|---|---|
| AUTH_INFO handler (~line 1041) | WAR3/W3XP → `logontype = 2`; others → `logontype = 0` |
| AUTH_INFO handler (~line 1063) | WAR3/W3XP → append 128-byte server signature |
| STARTGAME4 handler (~line 5429) | STAR/SEXP → different packet-length validation |
| CLOSEGAME handler (~line 5567) | WAR3/W3XP → skip `conn_set_game(NULL)` on CLOSEGAME |
| CHANGECLIENT handler (~line 6119) | Only valid for W3XP→WAR3 transition |
| ENTERCHAT / statstring | STAR/SEXP/SSHR use `"%s %u %u %u %u %u"` format; D2DV uses `"%s%s,%s,"` |
| GAMELISTREQ | `bngtype_to_gtype()` is clienttag-dependent |
| LADDERREQ | Ladder data keyed by `(clienttag, ladder_id)` |
| FILEINFOREQ | WAR3/W3XP → `icons-WAR3.bni`; STAR → `icons_STAR.bni` |

The [`src/common/tag.h`](src/common/tag.h) defines **17+ distinct client tags**
(STAR, SEXP, SSHR, DRTL, DSHR, W2BN, D2DV, JSTR, D2ST, D2XP, WAR3, W3XP,
plus WOL variants). The variant-specific logic is deeply intertwined with:

- **Domain logic** (ladder stats, game type mapping, character lists)
- **Application use cases** (auth, game lifecycle, profile)
- **Infrastructure** (file serving, version check selection)

None of this domain logic has been ported to the v3 layer yet.

---

## 2. Analysis

### Why variant sub-codecs are NOT needed for Phase E

1. **The wire format is not variant-specific at the SID level.**  
   STAR, D2DV, and WAR3 all use the same SID opcodes and the same packet
   headers. The differences are in *field semantics* (statstring format,
   logon type flag, server signature presence), not in packet framing or
   opcode assignment. The existing flat `ClientMessage` / `ServerMessage`
   variants already capture all wire bytes correctly.

2. **The current codec is already complete for Phase E purposes.**  
   [`decode_client()`](src/v3/protocol/bnet/include/protocol/bnet/codec.hpp:21)
   and [`decode_server()`](src/v3/protocol/bnet/include/protocol/bnet/codec.hpp:25)
   decode all known SIDs into typed structs. The `game_id` / `clienttag` field
   is preserved in the decoded struct and available to the FSM/use-case layer.

3. **Plan 06 §7 does not list variant sub-codecs as a Phase E task.**  
   The concrete task list (R250–R261) covers: DecodeError skeleton, message
   families (login, channel, game, friends, clan, ladder), IRC, Telnet, WoL,
   D2 s2s, and fuzz harnesses. Variant sub-codecs are described in §6 as
   *future state* with no assigned task number.

4. **The variant-specific logic is domain/application logic, not codec logic.**  
   The branching on `clienttag` in the legacy code is about:
   - Which auth flow to use (NLS vs OLS)
   - Which statstring format to parse/emit
   - Which game-type mapping to apply
   - Which ladder data to query
   
   These belong in the FSM, use-case, or domain layers — not in the pure
   codec. The codec's job is to turn bytes into typed structs; the FSM's job
   is to interpret those structs differently per client variant.

5. **Implementing now would require porting domain logic not yet available.**  
   A proper variant sub-codec (as described in Plan 06 §6) requires:
   - A `ClientTag` domain type that the codec can be parameterised on
   - Per-variant statstring parsers (domain logic)
   - Per-variant auth-flow selection (application logic)
   - A pump-level variant selector (integration layer)
   
   None of these are in scope for Phase E.

### Estimated scope if deferred

| Component | Estimated effort |
|---|---|
| `protocol/bnet/variants/star.hpp` + `star.cpp` | ~200 LOC |
| `protocol/bnet/variants/d2dv.hpp` + `d2dv.cpp` | ~300 LOC (statstring, char list) |
| `protocol/bnet/variants/war3.hpp` + `war3.cpp` | ~250 LOC (NLS auth, server sig) |
| `protocol/bnet/common.hpp` refactor | ~100 LOC |
| Pump-level variant selector | ~150 LOC |
| Unit + round-trip tests per variant | ~400 LOC |
| **Total** | **~1400 LOC** |

This is a medium-sized task, appropriate for a dedicated round (e.g. R290+)
after the FSM and use-case layers have been ported to v3.

---

## 3. Recommendation

**DEFER variant sub-codecs (STAR/D2DV/WAR3) to Phase F or a dedicated
post-Phase-E round.**

**Rationale:**
- The existing flat codec already correctly decodes all SID packets for all
  game variants. No correctness gap exists for Phase E.
- The variant-specific behaviour is domain/application logic, not wire-format
  logic. It belongs in the FSM and use-case layers, which are not yet ported.
- Plan 06 §7 does not assign a task number to variant sub-codecs, confirming
  they are out of scope for Phase E.
- Implementing now would require pulling in domain logic that is not yet
  available in the v3 layer, violating the "codec has zero dependency on
  domain types" rule stated in Plan 06 §2.

**Suggested future phase:** Plan 06 extension or a new Plan 06b, after:
- Phase F (FSM / session layer) is complete
- Domain types (`ClientTag`, `StatString`, auth-flow enums) are defined
- Use-case layer can provide the variant selector to the pump

---

## 4. Checklist

- [x] Read `codec.hpp` — confirmed: single flat decode, no per-game dispatch
- [x] Read `messages.hpp` — confirmed: unified `ClientMessage`/`ServerMessage` variants, no STAR/D2DV/WAR3-specific structs
- [x] Read `plans/06-protocol-and-codecs.md` — confirmed: variant sub-codecs are §6 future state, not in §7 task list
- [x] Check `src/v3/protocol/` for existing variant files — confirmed: none exist
- [x] Check legacy `handle_bnet_link.cpp` for variant scope — confirmed: extensive per-clienttag branching, deeply tied to domain/application logic
- [x] Formulate recommendation — **DEFER**
- [x] Create `plans/r280-checklist.md`
