# Plan 02 — `src/common/` Purge, Phase 2 (protocol headers)

> Started: 2026-06-02
> Plan: `plans/02-common-purge.md` (step 3, last bullet: `*_protocol.h` → `src/protocol/`)
> Status legend: ✅ Done | 🔄 In Progress | ⬜ Not Started

## Discovery (2026-06-02)

The legacy `*_protocol.h` wire-protocol headers in `src/common/` turned out to
be **dead code**, not live wire constants:

- **Zero v3 consumers.** No source outside `src/common/` `#include`s them. The
  apparent `src/protocol/bnet/*` references are doc comments
  (`// Mirrored from src/common/bnet_protocol.h`), not includes.
- **Uncompilable.** Every one `#include "common/bn_type.h"`, which Phase 1
  already relocated to `core/`. With the build green, no translation unit can
  be pulling them in.
- **Superseded.** The v3 `src/protocol/<family>/include/protocol/<family>/`
  layer already re-implements every wire type natively (`account_wire_types.hpp`,
  `auth_wire_types.hpp`, `chat_wire_types.hpp`, …).
- The only genuinely-compiled files in the `common` CMake target are the packet
  utilities (`packet*.cpp/.h`), `d2char_checksum.*`, and the Plan-08-deferred
  crypto. Those include only `field_sizes.h` / `lstr.h` / `setup_*.h` — never a
  protocol header.

**Decision (user, via dialog):** delete the dead headers rather than relocate
superseded code into `src/protocol/`.

## Plan for this phase

### Delete (dead, superseded) — ✅ DONE 2026-06-02
- [x] `anongame_protocol.h`
- [x] `bnet_protocol.h` + entire `bnet_protocol/` subdir (32 headers)
- [x] `bot_protocol.h`
- [x] `d2cs_bnetd_protocol.h`, `d2cs_d2gs_protocol.h`, `d2cs_protocol.h`,
      `d2game_protocol.h`
- [x] `file_protocol.h`, `init_protocol.h`, `irc_protocol.h`, `udp_protocol.h`,
      `wol_gameres_protocol.h`
- [x] `tracker.h`, `d2char_file.h`, `d2cs_d2dbs_ladder.h`, `d2cs_d2gs_character.h`

**49 files deleted** (16 top-level headers + 32 in `bnet_protocol/` + the dir + 1).
Exact: `git rm` of 16 headers + `git rm -r bnet_protocol/` (32 headers).

### Keep (live)
- `field_sizes.h`, `lstr.h` — included by compiled `packet_*.cpp`
- `packet*.{cpp,h}`, `packet_buffer/reader/writer.cpp`, `d2char_checksum.{cpp,h}`
- `setup_before.h`, `setup_after.h`
- crypto (`bnethash*`, `bnetsrp3`, `bigint`, `wolhash`, `bnethashconv`,
  `bnettime`) — deferred to Plan 08

### Follow-up
- [x] Updated `src/common/CMakeLists.txt` `COMMON_SOURCES` — dropped all deleted
      header entries; rewrote the leading comment to explain the deletion.
- [x] `cmake` reconfigure clean after deletions (Configuring/Generating done).
- [x] Sanity build: `test_protocol_common_headers_selfcontained` +
      `test_protocol_packet` build & pass (21 assertions / 7 cases). The v3
      protocol layer that supersedes the deleted headers is unaffected.
- [x] Noted residual `src/common/` contents below.

### Note on the `common` CMake target
`src/common/` is **not wired into the current build** — there is no
`add_subdirectory(common)` in `src/CMakeLists.txt`, so the `common` target does
not exist in this configuration and every `if(TARGET common)` block
(`infra_legacy_crypto`, legacy crypto tests) is inert. The deletions therefore
have zero effect on the active build; they only remove dead files from the tree.

### Residual `src/common/` after this phase (22 files)
- Live packet code: `packet.{cpp,h}`, `packet_buffer.cpp`, `packet_reader.cpp`,
  `packet_writer.cpp`
- Consumed constants: `field_sizes.h`, `lstr.h`
- `d2char_checksum.{cpp,h}`
- Compatibility: `setup_before.h`, `setup_after.h`
- Plan-08-deferred crypto: `bnethash*`, `bnethashconv*`, `bnetsrp3*`, `bigint*`,
  `wolhash*`
- `CMakeLists.txt`

Remaining Plan 02 work (future phases): relocate the live packet utilities to
`src/protocol/common/` (or `core/`), hand the crypto files to Plan 08, then
delete `src/common/` + its `CMakeLists.txt` entirely.

---

## Phase 2b — packet utilities (2026-06-02): ✅ resolved by DELETION

The "move live packet utilities to `src/protocol/common/`" sub-step turned out
to be another deletion: the legacy packet code was **also dead and already
superseded**.

- `packet_buffer.cpp` / `packet_reader.cpp` / `packet_writer.cpp` `#include`
  only each other + `common/packet.h` / `field_sizes.h` / `lstr.h` — all inside
  `src/common/`. **Zero external consumers** (repo-wide sweep of
  `src/`/`tests/`/`tools/`/`plugins/`).
- `d2char_checksum.{cpp,h}` had **zero references anywhere** except the CMake
  build entry — fully orphaned.
- `src/protocol/common/` already ships the native, header-only replacement,
  built as the **`protocol_common`** library (`packet.hpp`, `reader.hpp`,
  `writer.hpp`, `replay.hpp`, `frame_view.hpp`, `next_frame.hpp`) — references
  no legacy `common/`. Moving the legacy code would duplicate it.

**Decision (user, via dialog): delete.**

### Deleted (9 files)
- [x] `packet.cpp`, `packet.h`
- [x] `packet_buffer.cpp`, `packet_reader.cpp`, `packet_writer.cpp`
- [x] `field_sizes.h`, `lstr.h`
- [x] `d2char_checksum.cpp`, `d2char_checksum.h`

### Kept
- `setup_before.h` / `setup_after.h` — still `#include`d by `src/win32/*` and
  `src/infra/legacy_crypto/`; retire with those callers.
- Plan-08 crypto: `bnethash*`, `bnethashconv*`, `bnetsrp3*`, `bigint*`,
  `wolhash*`.

### Verification
- [x] Repo-wide sweep: no external `#include` of any of the 9 files.
- [x] `src/common/CMakeLists.txt` pruned to setup + crypto + pugixml.
- [x] CMake reconfigures clean.
- [x] Native replacement green: `test_protocol_{packet,reader,writer,replay}`
      — 136 assertions / 26 cases pass.

### `src/common/` now (13 files)
`setup_before.h`, `setup_after.h`, `CMakeLists.txt`, and the 10 Plan-08 crypto
files (`bnethash.{cpp,h}`, `bnethashconv.{cpp,h}`, `bnetsrp3.{cpp,h}`,
`bigint.{cpp,h}`, `wolhash.{cpp,h}`). Started Phase 2 at 44 files → now 13.

### Remaining to fully retire `src/common/`
- Plan 08: migrate crypto → `src/infra/crypto/`, delete the legacy crypto files.
- Retire `setup_*.h`: port `src/win32/*` + `infra/legacy_crypto` off them.
- Then delete `src/common/` + `CMakeLists.txt`.

---

## Phase 2c — `setup_*.h` shim (2026-06-02): PARTIAL (infra cleaned; deletion gated)

Goal was to retire `common/setup_before.h` / `setup_after.h` so `src/common/`
reaches crypto-only. Audit found `setup_*.h` is still consumed by:

| Consumer | Count | Disposition |
|---|---|---|
| `src/common/*` crypto `.cpp` (bigint, bnethash, bnethashconv, bnetsrp3, wolhash) | 5 | **Plan 08** — leaves when crypto migrates to `src/infra/crypto/` |
| `src/infra/legacy_crypto/src/bnet_session_hasher.cpp` | 1 | ✅ **cleaned this session** |
| `src/win32/*` (GUI) | 9 | legacy Windows GUI, not built on Linux — out of scope |

So `setup_*.h` **cannot be deleted yet**: both the Plan-08 crypto files and the
win32 GUI still need it. `src/common/` therefore stays at "crypto + setup shim"
until Plan 08 (crypto) and the win32 retirement land.

### Done
- [x] `bnet_session_hasher.cpp`: dropped the unnecessary
      `common/setup_before.h` + `common/setup_after.h` wrap (the TU uses no
      compile-time config macros; `bnethash.h` is self-contained). Its last
      `common/` include (`bnethash.h`) is a Plan-08 item.
- [x] **`src/infra/` is now free of `common/setup_*`** (verified by grep).
- [x] CMake reconfigures clean.

### Blocked / out of scope
- `setup_*.h` deletion — gated on Plan 08 (crypto) + win32 GUI retirement.

## Log
- 2026-06-02: deleted 49 dead `*_protocol.h` headers (Phase 2).
- 2026-06-02: deleted 9 dead packet-utility files (Phase 2b); native
  `protocol_common` confirmed green.
- 2026-06-02: cleaned `setup_*.h` out of `src/infra/legacy_crypto` (Phase 2c);
  full `setup_*.h` deletion gated on Plan 08 + win32.

## Safety verification (pre-delete)
- ✅ Repo-wide sweep (`src/`, `tests/`, `tools/`, `plugins/`): no genuine
  `#include` of any delete-candidate outside `src/common/`.
- ✅ KEEP files (`field_sizes.h`, `lstr.h`) include no delete-candidate.
- ✅ `cmake/layering_exceptions.txt` already has zero `common` entries.

## Log
- 2026-06-02: discovery + decision recorded; beginning deletions.
