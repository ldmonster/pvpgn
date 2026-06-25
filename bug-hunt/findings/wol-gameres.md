# Bug Hunt: Westwood Online (WOL) Game Results + Ladder + SKU Handling

Subsystem: WOL "gameres" packet parsing, per-SKU result handling, stat/ladder
attribution, WOL ladder reply.

- ORIGINAL: `/home/cnupt/work/pvpgn-server`
- CURRENT v3: `/home/cnupt/work/pvpgn`

## Overall implementation status

The WOL game-result subsystem is **almost entirely NOT IMPLEMENTED in v3**.

Original (`src/bnetd/handle_wol_gameres.cpp`, 3338 lines) does the full job:
- Parses the gameres TLV stream (header + tag/type/len/value records).
- ~130 per-tag handlers covering game-resolution tags, Renegade per-player
  tags, and indexed per-slot tags (NAM0..7, CMP0..7, etc.).
- Performs **stat/ladder attribution**: resolves the game by IDNO, identifies
  the sender account (SPID + NAM), maps each slot's CMPx win/loss/disconnect
  bitfield into a `t_game_result`, then commits via
  `game_set_reported_results()` / `game_set_report()`.

v3 has only:
1. `src/protocol/wolgameres/` — a **pure TLV codec** (`codec.cpp`) plus a tag
   constants header (`wire_types.hpp`). The codec frames/deframes the packet
   into `Report{header, entries[]}` but assigns **no tag semantics**.
2. `src/protocol/wolgameres/src/wol_fsm.cpp` — a **stub** FSM
   (`on_bytes`/`handle_game_report` both `(void)bytes; return ok();`).

What MATCHES (verified correct in the v3 codec):
- Header layout: `u16 size` (BE) then `u16 rngd_size` (BE). Matches
  `t_wolgameres_header` and `bn_short_nget` usage.
- Optional 4-byte zero prefix (RNDG marker) before the TLV list — matches
  original lines 702-703.
- TLV record order: `u32 tag` (BE), `u16 data_type` (BE), `u16 data_len` (BE),
  `data_len` bytes — matches original lines 712-733.
- Data-type codes: BYTE=1, BOOL=2, TIME=5, INT=6, STRING=7, BIGINT=20 — match
  `common/wol_gameres_protocol.h`.
- All ~120 tag FourCC constants in `wire_types.hpp` match the original
  `CLIENT_*_UINT` values exactly (spot-checked SERN/GSKU/IDNO/PNAM/etc.).

---

## Finding 1 — WOL game-result handling, ladder, SKU dispatch NOT IMPLEMENTED

- Severity: High (feature absent) — but the live FSM is a no-op, so no
  runtime corruption; it silently drops every WOL game report.
- Classification: NOT-IMPLEMENTED
- Original ref: `handle_wol_gameres.cpp:688-791` `handle_wol_gameres_packet()`
  drives the whole flow and commits results:
  ```
  game_set_report(gameres_result->game, gameres_result->myaccount, "head", "body");
  if (game_set_reported_results(... gameres_result->results) < 0) xfree(...);
  ```
  Plus the ~130 `_client_*` handlers + `handle_wolgameres_tag` dispatch table.
- v3 ref: `src/protocol/wolgameres/src/wol_fsm.cpp:9-28`
  ```
  core::Status<> WolFsm::on_bytes(...) { ... (void)bytes; return core::ok(); }
  core::Status<> WolFsm::handle_game_report(...) { (void)packet; return core::ok(); }
  ```
- Divergence: No tag dispatch, no game lookup (IDNO), no sender resolution
  (SPID/NAM), no CMPx win/loss mapping, no ladder commit, no SKU validation.
  The codec produces `Report.entries` but nothing consumes them.
- Proposed fix: Implement `handle_game_report` to (a) `decode()` the buffer,
  (b) locate the game by the `IDNO` tag, (c) resolve sender via `SPID`+`NAM<n>`,
  (d) map each `CMP<n>` entry to win/loss/disconnect (see Finding 4), (e) commit
  through the game/ladder repository. Until then, document it as unsupported.

## Finding 2 — gameres FSM is never wired into the live session

- Severity: High (dead code) — the stub above is unreachable regardless.
- Classification: NOT-IMPLEMENTED / dead-code
- v3 ref: `src/app/bnetd/src/main/wol_session.cpp:23`
  ```
  auto fsm = std::make_shared<protocol::wol::WolFsm>(ctx);   // chat/auth FSM
  ```
  This constructs `protocol::wol::WolFsm` from `protocol/wol/wol_fsm.hpp`
  (the **chat/auth** FSM), NOT the gameres FSM. The gameres class is also named
  `protocol::wol::WolFsm` but is declared in
  `protocol/wolgameres/include/protocol/wolgameres/wol_fsm.hpp` and takes
  `(ctx, IGameRepository, IEventBus)`. A grep for any `IGameRepository`-taking
  `WolFsm` constructor across `src/app` and `src/integration` finds **zero**
  call sites — the gameres FSM is never instantiated anywhere.
- Note the name collision: two distinct classes both named
  `pvpgn::protocol::wol::WolFsm` (chat vs gameres) is confusing and a latent
  ODR/maintenance hazard.
- Divergence: Original routes WOL gameres packets to
  `handle_wol_gameres_packet`. v3 has no routing into the gameres handler.
- Proposed fix: Rename the gameres class (e.g. `WolGameresFsm`) and wire it
  into the session dispatch once Finding 1 is implemented.

## Finding 3 — Codec does NOT 4-byte-align/pad record advance (latent BUG once wired)

- Severity: Medium (latent — only bites once the codec is fed real packets).
- Classification: BUG (divergence in framing)
- Original ref: `handle_wol_gameres.cpp:764-765`
  ```
  datalen = 4 * ((datalen + 3) / 4);   // round data length up to 4-byte boundary
  offset += datalen;
  ```
  The original advances the read cursor past the value **rounded up to a
  multiple of 4 bytes** (wire records are padded to 4-byte alignment).
- v3 ref: `src/protocol/wolgameres/src/codec.cpp:74-84`
  ```
  auto len = r.read_be<std::uint16_t>();
  auto data = r.read_bytes(len.value());   // reads exactly len, no padding skip
  ```
  v3 reads exactly `data_len` bytes and immediately reads the next tag.
- Divergence: For any record whose `data_len` is not a multiple of 4 (e.g. a
  3-byte or 5-byte string), the original skips padding bytes that v3 would
  mis-interpret as the start of the next tag — desynchronizing the whole TLV
  walk. v3's `decode()` would then read garbage tags/types and either fail
  ("unknown data_type") or silently produce wrong entries.
- Why tests don't catch it: the unit tests in
  `tests/unit/protocol/wolgameres/codec_test.cpp` only use `data_len` values of
  0 or multiples of 4 (the round-trip encode also emits unpadded), so encode and
  decode agree with each other but **disagree with the real WOL wire format**.
- Proposed fix: After `read_bytes(len)`, skip `(-len) & 3` padding bytes:
  `r.skip((4 - (len & 3)) & 3)`. Mirror the padding in `encode()` so round-trip
  still holds. Add a test with an odd-length (e.g. 3-byte) string entry.

## Finding 4 — CMPx win/loss/disconnect bitfield semantics not carried into the codec layer (informational)

- Severity: Low (informational; relevant when implementing Finding 1).
- Classification: NOT-IMPLEMENTED
- Original ref: `handle_wol_gameres.cpp:2391-2452` `_cl_cmp_general()`:
  ```
  resultnum &= 0x0000FF00; resultnum >>= 8;   // 0x01=WIN 0x02=LOSE 0x03=DISCONNECT
  ... results[i] = (1->win, 2->loss, 3->disconnect, default->disconnect);
  ```
  Slot index `i` is found by matching `game_get_player(game,i) == other_account`,
  where `other_account` was set by the preceding `NAM<n>` tag
  (`_cl_nam_general`, lines 2109-2124). Attribution is therefore **ordering
  dependent**: each `NAM<n>` sets `otheraccount`, the following `CMP<n>` consumes
  it. `myaccount` is the slot whose index == `senderid` (from `SPID`).
- v3 ref: tags exist as constants (`kClientCmp`, `kClientNam`, `kClientSpid` in
  `wire_types.hpp`) but **no handler maps them to a result**. The codec returns
  raw `Entry` blobs only.
- Proposed fix: When implementing Finding 1, replicate the
  `0x0000FF00 >> 8` mask and the NAM-then-CMP ordering coupling, plus the
  SPID-based sender identification. Note also the original's
  `default -> disconnect` fallback for unknown result codes.

## Finding 5 — `read_bigint` semantics differ from original (informational, unused)

- Severity: Low (informational; helper currently unused).
- Classification: UNSURE / divergence (no live caller)
- Original ref: `handle_wol_gameres.cpp:630-657` `wol_gameres_get_long_from_data`
  treats a BIGINT as a **sum of consecutive 4-byte BE ints** across the buffer,
  gated on `size >= 4`, returning an `unsigned int` (32-bit accumulator). This
  is itself a quirky/odd implementation (it accumulates rather than concatenates).
- v3 ref: `src/protocol/wolgameres/src/codec.cpp:132-139` `read_bigint` reads a
  single 8-byte BE value, gated on `size >= 8`.
- Divergence: Different gating threshold (8 vs 4) and different arithmetic
  (single 64-bit read vs 32-bit accumulation). Neither is wired to a caller in
  v3, and the original's accumulation looks buggy itself, so this is flagged for
  awareness rather than as a confirmed defect. Verify against a real BIGINT-tag
  packet before matching either behavior.

## Finding 6 — WOL ladder reply / anongame_wol NOT IMPLEMENTED

- Severity: Medium (feature absent).
- Classification: NOT-IMPLEMENTED
- Original ref: `src/bnetd/anongame_wol.cpp` (615 lines) — WOL anongame
  matchmaking/ladder player tag table and match list (`_handle_address_tag`,
  `_handle_port_tag`, `_handle_country_tag`, `anongame_wol_matchlist_head`,
  etc.).
- v3 ref: no equivalent. `src/application/anongame_inforeply` and
  `anongame_lobby` exist but contain no WOL ladder-reply logic; grep for the WOL
  anongame tag handling finds nothing.
- Proposed fix: Port the WOL anongame match/ladder reply once the gameres
  pipeline (Findings 1-2) lands; until then mark WOL ladder unsupported.

---

## Summary table

| # | Area | Severity | Class |
|---|------|----------|-------|
| 1 | gameres handling/ladder/SKU dispatch | High | NOT-IMPLEMENTED |
| 2 | gameres FSM not wired into session (dead code, name clash) | High | NOT-IMPLEMENTED |
| 3 | codec missing 4-byte record padding/alignment | Medium | BUG (latent) |
| 4 | CMPx win/loss bitfield + NAM/SPID attribution | Low | NOT-IMPLEMENTED |
| 5 | read_bigint semantics differ (unused) | Low | UNSURE |
| 6 | WOL anongame ladder reply | Medium | NOT-IMPLEMENTED |

Net: the v3 WOL gameres codec (framing + tag table) is correct and matches the
original wire format, **except** it omits the 4-byte record padding the original
applies (Finding 3 — the one concrete code bug). Everything above the codec
(tag semantics, SKU validation, stat/ladder attribution, ladder reply) is
unimplemented, and even the existing gameres FSM stub is never wired into the
live session.
