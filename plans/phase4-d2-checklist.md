# Phase 4 — D2 Services Migration Checklist

> **Created:** Round 143 (2026-05-22)  
> **Reference:** [`plans/refactoring-plan-legacy-d2.md`](plans/refactoring-plan-legacy-d2.md)  
> **Depends on:** Phase 3 (in progress — D2 work is independent of bnetd handler deletion)

---

## Overview

Migrate `src/d2cs/` (42 files, ~8 700 LOC) and `src/d2dbs/` (20 files, ~4 100 LOC) into
the v3 hexagonal architecture. Both services already have static-library carve-outs
(`d2cs_legacy`, `d2dbs_legacy`) and v3 composition-root stubs. The `D2CSSessionFsm`
(310-line header, 706-line impl) is already complete from R129.

The migration follows the same strangler-fig pattern used for bnetd:
1. Identify what already exists in v3
2. Fill gaps in domain / application / protocol layers
3. Wire composition roots
4. Delete legacy files once v3 owns each subsystem

---

## Legacy File Inventory

### src/d2cs/ — 42 files, ~8 700 LOC

| File | Lines | Role | v3 Target | Status |
|------|------:|------|-----------|--------|
| `handle_d2cs.cpp` | 1357 | Client packet dispatch (inbound) | `protocol/d2cs/fsm.cpp` | 🔄 FSM exists; wiring pending |
| `prefs.cpp` | 1265 | Config loading | `infra/config/d2cs_config.hpp` | ❌ Not started |
| `d2charfile.cpp` | 878 | Character save-file I/O | `infra/persistence/realm/filesystem_save_store.cpp` | 🔄 Partial (FilesystemSaveStore exists) |
| `connection.cpp` | 803 | Per-client connection state | `domain/connection/` + `app/d2cs/` | ❌ Not started |
| `handle_d2gs.cpp` | 642 | D2GS packet dispatch | `protocol/d2gs/` + `application/realm/gs_queue.cpp` | 🔄 Codec exists; FSM pending |
| `game.cpp` | 512 | Game record management | `domain/realm/` + `application/realm/` | 🔄 Partial |
| `d2gs.cpp` | 453 | D2GS connection management | `application/realm/gs_queue.cpp` | 🔄 GsQueue exists |
| `handle_bnetd.cpp` | 374 | bnetd↔d2cs protocol | `protocol/d2cs/bnetd_wire_types.hpp` | 🔄 Wire types exist |
| `cmdline.cpp` | 372 | CLI argument parsing | `services/d2cs/` composition root | ❌ Not started |
| `server.cpp` | 332 | Event loop + accept loop | `app/d2cs/asio_event_loop` | ❌ Not started |
| `main.cpp` | 291 | Entry point | `services/d2cs/` composition root | ❌ Not started |
| `d2ladder.cpp` | 259 | Ladder data management | `domain/realm/` + `infra/persistence/realm/` | ❌ Not started |
| `net.cpp` | 242 | Low-level socket helpers | `infra/net/` | ❌ Not started |
| `handle_signal.cpp` | 209 | Signal handling | `services/d2cs/` composition root | ❌ Not started |
| `gamequeue.cpp` | 194 | Game creation queue | `application/realm/gs_queue.cpp` | 🔄 GsQueue exists |
| `serverqueue.cpp` | 166 | Server queue management | `application/realm/gs_queue.cpp` | 🔄 GsQueue exists |
| `s2s.cpp` | 149 | Server-to-server protocol | `protocol/d2cs/` + `infra/net/` | ❌ Not started |
| `d2charlist.cpp` | 102 | Character list management | `domain/realm/character_list.cpp` | 🔄 CharacterList exists |
| `handle_init.cpp` | 99 | Connection classifier | `protocol/d2cs/fsm.cpp` | 🔄 FSM handles init |
| `bnetd.cpp` | 93 | bnetd connection management | `application/realm/` | ❌ Not started |
| `prefs.h` | 97 | Config declarations | `infra/config/d2cs_config.hpp` | ❌ Not started |
| `connection.h` | 150 | Connection type declarations | `domain/connection/` | ❌ Not started |
| `game.h` | 137 | Game type declarations | `domain/realm/` | 🔄 Partial |
| `d2charfile.h` | 116 | Character file declarations | `infra/persistence/realm/` | 🔄 Partial |
| `setup.h` | 123 | Build config shims | (delete after migration) | ❌ Not started |
| `d2gs.h` | 85 | D2GS declarations | `application/realm/gs_queue.hpp` | 🔄 Partial |
| `serverqueue.h` | 60 | Server queue declarations | `application/realm/gs_queue.hpp` | 🔄 Partial |
| `gamequeue.h` | 54 | Game queue declarations | `application/realm/gs_queue.hpp` | 🔄 Partial |
| `d2ladder.h` | 51 | Ladder declarations | `domain/realm/` | ❌ Not started |
| `handle_d2cs.h` | 38 | Client handler declarations | (delete with handle_d2cs.cpp) | 🔄 FSM replaces |
| `handle_d2gs.h` | 37 | D2GS handler declarations | (delete with handle_d2gs.cpp) | ❌ Not started |
| `handle_bnetd.h` | 37 | bnetd handler declarations | (delete with handle_bnetd.cpp) | 🔄 Wire types exist |
| `server.h` | 37 | Server declarations | `app/d2cs/` | ❌ Not started |
| `handle_init.h` | 36 | Init handler declarations | (delete with handle_init.cpp) | 🔄 FSM replaces |
| `handle_signal.h` | 43 | Signal handler declarations | (delete with handle_signal.cpp) | ❌ Not started |
| `net.h` | 38 | Net declarations | `infra/net/` | ❌ Not started |
| `s2s.h` | 38 | S2S declarations | `protocol/d2cs/` | ❌ Not started |
| `bnetd.h` | 40 | bnetd declarations | `application/realm/` | ❌ Not started |
| `d2charlist.h` | 39 | Character list declarations | `domain/realm/character_list.hpp` | 🔄 Exists |
| `cmdline.h` | 47 | CLI declarations | `services/d2cs/` | ❌ Not started |
| `bit.h` | 35 | Bit manipulation helpers | `core/` or inline | ❌ Not started |
| `version.h` | — | Version string | `services/d2cs/` | ❌ Not started |

**Summary:** 42 files · ~8 700 LOC · 12 files have partial v3 coverage · 30 files not started

### src/d2dbs/ — 20 files, ~4 100 LOC

| File | Lines | Role | v3 Target | Status |
|------|------:|------|-----------|--------|
| `d2ladder.cpp` | 906 | Ladder data management | `domain/realm/` + `infra/persistence/realm/` | ❌ Not started |
| `dbspacket.cpp` | 900 | D2DBS packet dispatch | `protocol/d2dbs/fsm.cpp` | 🔄 FSM stub exists |
| `prefs.cpp` | 545 | Config loading | `infra/config/d2dbs_config.hpp` | ❌ Not started |
| `dbserver.cpp` | 504 | Event loop + accept loop | `app/d2dbs/asio_event_loop` | ❌ Not started |
| `cmdline.cpp` | 372 | CLI argument parsing | `services/d2dbs/` composition root | ❌ Not started |
| `charlock.cpp` | 254 | Character lock management | `application/realm/character_lock.cpp` | 🔄 CharacterLock exists |
| `main.cpp` | 267 | Entry point | `services/d2dbs/` composition root | ❌ Not started |
| `handle_signal.cpp` | 185 | Signal handling | `services/d2dbs/` composition root | ❌ Not started |
| `dbsdupecheck.cpp` | 106 | Duplicate character check | `domain/realm/dupe_checker.cpp` | 🔄 DupeChecker exists |
| `dbspacket.h` | 127 | Packet declarations | (delete with dbspacket.cpp) | 🔄 FSM stub exists |
| `prefs.h` | 59 | Config declarations | `infra/config/d2dbs_config.hpp` | ❌ Not started |
| `d2ladder.h` | 72 | Ladder declarations | `domain/realm/` | ❌ Not started |
| `dbserver.h` | 61 | Server declarations | `app/d2dbs/` | ❌ Not started |
| `charlock.h` | 56 | Character lock declarations | `application/realm/character_lock.hpp` | 🔄 Exists |
| `setup.h` | 54 | Build config shims | (delete after migration) | ❌ Not started |
| `cmdline.h` | 47 | CLI declarations | `services/d2dbs/` | ❌ Not started |
| `handle_signal.h` | 42 | Signal handler declarations | (delete with handle_signal.cpp) | ❌ Not started |
| `dbsdupecheck.h` | 32 | Dupe check declarations | `domain/realm/dupe_checker.hpp` | 🔄 Exists |
| `version.h` | — | Version string | `services/d2dbs/` | ❌ Not started |

**Summary:** 20 files · ~4 100 LOC · 5 files have partial v3 coverage · 15 files not started

---

## What Already Exists in v3

### Domain Layer (`src/v3/domain/realm/`)

| File | Lines | Covers |
|------|------:|--------|
| `include/domain/realm/character.hpp` | — | Character aggregate |
| `src/character.cpp` | — | Character impl |
| `include/domain/realm/character_list.hpp` | — | Character list aggregate |
| `src/character_list.cpp` | — | CharacterList impl |
| `include/domain/realm/dupe_checker.hpp` | — | Duplicate character detection |
| `src/dupe_checker.cpp` | — | DupeChecker impl |
| `include/domain/realm/realm.hpp` | — | Realm aggregate |

**Gap:** No `D2Ladder` domain object. `d2cs/d2ladder.cpp` (259 LOC) and
`d2dbs/d2ladder.cpp` (906 LOC) are both unmigratable until this exists.

### Application Layer (`src/v3/application/realm/`)

| File | Covers |
|------|--------|
| `character_lock.hpp` / `.cpp` | Maps to `d2dbs/charlock.cpp` |
| `character_persistence.hpp` / `.cpp` | Maps to `d2cs/d2charfile.cpp` (partial) |
| `create_character.hpp` / `.cpp` | Maps to `handle_d2cs.cpp` CREATECHARREQ |
| `delete_character.hpp` / `.cpp` | Maps to `handle_d2cs.cpp` DELETECHARREQ |
| `list_characters.hpp` / `.cpp` | Maps to `handle_d2cs.cpp` CHARLISTREQ |
| `load_character.hpp` / `.cpp` | Maps to `handle_d2cs.cpp` CHARLOGINREQ |
| `save_character.hpp` / `.cpp` | Maps to `d2dbs/dbspacket.cpp` save path |
| `gs_queue.hpp` / `.cpp` | Maps to `d2cs/gamequeue.cpp` + `serverqueue.cpp` |
| `join_game_server.hpp` / `.cpp` | Maps to `handle_d2cs.cpp` JOINGAMEREQ |
| `d2_ladder_repository.hpp` | Interface only — no impl yet |
| `character_list_repository.hpp` | Interface only — no impl yet |

**Gap:** `d2_ladder_repository.hpp` has no implementation. `character_list_repository.hpp`
has no implementation. Both are needed before Step 2 use-cases can be wired.

### Protocol Layer

| Module | Files | Covers | Status |
|--------|-------|--------|--------|
| `protocol/d2cs/` | `fsm.hpp` (310L), `fsm.cpp` (706L), `codec.hpp/cpp`, `wire_types.hpp`, `bnetd_wire_types.hpp`, `charlistreply_encoder.hpp`, `ladderreply_encoder.hpp` | All client→D2CS packets; all D2CS→client replies | ✅ Complete |
| `protocol/d2dbs/` | `fsm.hpp`, `fsm.cpp`, `codec.hpp/cpp`, `wire_types.hpp` | D2DBS wire protocol | ✅ Complete (R148) |
| `protocol/d2gs/` | `codec.hpp/cpp` | D2GS wire protocol | 🔄 Codec only — no FSM |
| `protocol/d2save/` | — | D2 save-file codec | ✅ Complete |

### Infrastructure Layer (`src/v3/infra/persistence/realm/`)

| File | Covers |
|------|--------|
| `filesystem_save_store.hpp` / `.cpp` | Maps to `d2cs/d2charfile.cpp` (file I/O) |
| `inmemory_character_repository.hpp` / `.cpp` | In-memory character store |
| `inmemory_save_store.hpp` / `.cpp` | In-memory save store |

**Gap:** No `FilesystemLadderStore`. `d2cs/d2ladder.cpp` and `d2dbs/d2ladder.cpp`
write ladder data to disk; this needs a v3 implementation.

### Composition Roots (`src/v3/services/`)

| Module | Files | Status |
|--------|-------|--------|
| `services/d2cs/` | `d2cs_composition.hpp`, `d2cs_composition.cpp` | Stub — empty |
| `services/d2dbs/` | `d2dbs_composition.hpp`, `d2dbs_composition.cpp` | Stub — empty |

---

## Migration Steps

### Step 1 — Domain Layer Gaps ❌ Not started

Fill the two missing domain objects needed by all subsequent steps.

#### 1a. `D2Ladder` domain object

**Target:** `src/v3/domain/realm/include/domain/realm/d2ladder.hpp` + `src/d2ladder.cpp`

Legacy source: `src/d2cs/d2ladder.cpp` (259 LOC) + `src/d2dbs/d2ladder.cpp` (906 LOC)

The legacy ladder stores entries in a flat binary file. The v3 domain object should:
- Define `LadderEntry` value type (account name, character name, level, class, XP, status flags)
- Define `D2Ladder` aggregate with `add_entry()`, `remove_entry()`, `get_top_n()`, `find_by_char()`
- Be persistence-agnostic (no file I/O in domain)

Checklist:
- [ ] Define `LadderEntry` struct in `d2ladder.hpp`
- [ ] Define `D2Ladder` aggregate with in-memory sorted storage
- [ ] Unit tests: add/remove/query/sort (target: 20+ tests)
- [ ] Wire `D2LadderRepository` interface in `application/realm/d2_ladder_repository.hpp`

#### 1b. `CharacterListRepository` implementation

**Target:** `src/v3/infra/persistence/realm/include/infra/persistence/realm/inmemory_character_list_repository.hpp`

The `application/realm/character_list_repository.hpp` interface exists but has no
implementation. The `InMemoryCharacterRepository` covers individual characters but not
the ordered list (which maps to `d2cs/d2charlist.cpp`).

Checklist:
- [ ] Implement `InMemoryCharacterListRepository` backed by `std::vector<CharacterEntry>`
- [ ] Implement `FilesystemCharacterListRepository` (reads `.d2s` directory listing)
- [ ] Unit tests: list/add/remove/reorder (target: 15+ tests)

---

### Step 2 — D2CS Domain Layer ✅ Complete (R145)

The D2CS domain layer has been created with types, repository interfaces,
use cases, and in-memory implementations for testing.

#### 2a. D2CS domain types and use cases (R145)

Checklist:
- [x] `domain/d2cs/types.hpp` — `CharacterClass`, `CharacterFlags`, `CharacterInfo`, `LadderType`, `LadderEntry`, `GameInfo`, `RealmLogonResult`
- [x] `domain/d2cs/character_repository.hpp` — `ICharacterRepository` interface
- [x] `domain/d2cs/ladder_repository.hpp` — `ILadderRepository` interface
- [x] `domain/d2cs/use_cases.hpp` — `CharacterListUseCase`, `CharacterSelectUseCase`, `CharacterCreateUseCase`, `CharacterDeleteUseCase`, `LadderQueryUseCase`
- [x] `domain/d2cs/in_memory_repositories.hpp` — `InMemoryCharacterRepository`, `InMemoryLadderRepository`
- [x] `domain/d2cs/CMakeLists.txt` — `domain_d2cs` interface library
- [x] `src/v3/CMakeLists.txt` updated — `add_subdirectory(domain/d2cs)`
- [x] `tests/unit/domain/d2cs/use_cases_test.cpp` — 25 test cases, 90+ assertions
- [x] `tests/unit/domain/d2cs/CMakeLists.txt` — test target registered
- [x] `tests/unit/domain/CMakeLists.txt` updated — `add_subdirectory(d2cs)`

### Step 2 (application) — Application Layer Completion ❌ Not started

Wire the existing use-case services to the new domain objects and fill remaining gaps.

#### 2b. D2CS application use-case gaps

| Use Case | Legacy Source | Status |
|----------|--------------|--------|
| `CreateGame` | `d2cs/game.cpp` + `handle_d2cs.cpp` CREATEGAMEREQ | ❌ Missing |
| `JoinGame` | `d2cs/game.cpp` + `handle_d2cs.cpp` JOINGAMEREQ | 🔄 `JoinGameServer` exists (partial) |
| `CancelCreateGame` | `d2cs/gamequeue.cpp` CANCELCREATEGAME | ❌ Missing |
| `GetGameList` | `d2cs/game.cpp` GAMELISTREQ | ❌ Missing |
| `GetGameInfo` | `d2cs/game.cpp` GAMEINFOREQ | ❌ Missing |
| `GetMotd` | `d2cs/handle_d2cs.cpp` MOTDREQ | ❌ Missing |
| `ConvertCharacter` | `d2cs/d2charfile.cpp` CONVERTCHARREQ | ❌ Missing |
| `GetLadder` | `d2cs/d2ladder.cpp` LADDERREQ | ❌ Missing (needs Step 1a) |
| `AuthenticateWithBnetd` | `d2cs/bnetd.cpp` + `handle_bnetd.cpp` | ❌ Missing |

Checklist:
- [ ] `application/realm/create_game.hpp` + `.cpp` — wraps `GsQueue::enqueue_game()`
- [ ] `application/realm/cancel_create_game.hpp` + `.cpp`
- [ ] `application/realm/get_game_list.hpp` + `.cpp`
- [ ] `application/realm/get_game_info.hpp` + `.cpp`
- [ ] `application/realm/get_motd.hpp` + `.cpp`
- [ ] `application/realm/convert_character.hpp` + `.cpp`
- [ ] `application/realm/get_ladder.hpp` + `.cpp` (depends on Step 1a)
- [ ] `application/realm/authenticate_with_bnetd.hpp` + `.cpp`
- [ ] Unit tests for each use case (target: 8 × 5 = 40+ tests)

#### 2b. D2DBS use-case gaps

| Use Case | Legacy Source | Status |
|----------|--------------|--------|
| `EchoRequest` | `d2dbs/dbspacket.cpp` D2DBS_D2CS_ECHOREQ | ❌ Missing |
| `SaveCharacter` | `d2dbs/dbspacket.cpp` D2DBS_D2CS_CHARLOCK | 🔄 `SaveCharacter` exists (partial) |
| `LoadCharacter` | `d2dbs/dbspacket.cpp` D2DBS_D2CS_CHARLOCK | 🔄 `LoadCharacter` exists (partial) |
| `LockCharacter` | `d2dbs/charlock.cpp` | 🔄 `CharacterLock` exists |
| `UnlockCharacter` | `d2dbs/charlock.cpp` | 🔄 `CharacterLock` exists |
| `CheckDuplicate` | `d2dbs/dbsdupecheck.cpp` | 🔄 `DupeChecker` exists |
| `UpdateLadder` | `d2dbs/d2ladder.cpp` | ❌ Missing (needs Step 1a) |

Checklist:
- [ ] `application/realm/echo_request.hpp` + `.cpp`
- [ ] `application/realm/update_ladder.hpp` + `.cpp` (depends on Step 1a)
- [ ] Expand `SaveCharacter` / `LoadCharacter` to cover full `dbspacket.cpp` paths
- [ ] Unit tests for each use case (target: 7 × 5 = 35+ tests)

---

### Step 3 — Protocol FSM Wiring 🔄 Partial

#### 3a. D2CS FSM → Application Layer

**R144 audit result:** `D2CSSessionFsm` now covers all 15 client→D2CS packet types
with full callbacks. Three gaps were closed:

| Gap | Fix |
|-----|-----|
| `LADDERREQ` (0x11) silently ignored | Added `D2CSLadderRequest` struct + `on_ladder` callback + `handle_ladder` (reads `ladder_type:u8`, `start_pos:u16le`) |
| `CHARLADDERREQ` (0x16) silently ignored | Added `D2CSCharLadderRequest` struct + `on_char_ladder` callback + `handle_char_ladder` (reads `hardcore:u32`, `expansion:u32`, `char_name:cstr`) |
| `CHARLISTREQ110` (0x19) shared handler, no separate callback | Added `on_char_list_110` callback; `handle_char_list_110` fires `on_char_list` first (backward-compat) then `on_char_list_110` |

New helpers added: `read_u8()`, `read_u16le()`.
Test suite expanded from 40 → 62 test cases (TC-41 through TC-62).
`wire_types.hpp` required no changes — all 15 packet type codes were already present.

The FSM is now complete at the wire-protocol level. The callbacks need to be wired to
the application use-cases from Step 2.

**Target:** `src/v3/app/d2cs/include/app/d2cs/d2cs_session_handler.hpp` + `.cpp`

This mirrors `BnetConnectionAdapter` from R140: it implements the `D2CSFsmCallbacks`
interface and delegates each callback to the appropriate use-case service.

Checklist:
- [x] Audit FSM against legacy `handle_d2cs.cpp` — gap list produced (R144)
- [x] Add `on_ladder` / `handle_ladder` for LADDERREQ (R144)
- [x] Add `on_char_ladder` / `handle_char_ladder` for CHARLADDERREQ (R144)
- [x] Add `on_char_list_110` / `handle_char_list_110` for CHARLISTREQ110 (R144)
- [x] Add `read_u8()` and `read_u16le()` helpers (R144)
- [x] Expand test suite to 62 test cases (R144)
- [x] Create `D2CSSessionHandler` implementing `D2CSFsmCallbacks` (R146)
- [x] Create `ID2CSSessionEgress` outbound port interface (R146)
- [x] Wire `on_login` → stub (always Success; real auth in later round) (R146)
- [x] Wire `on_char_login` → `CharacterSelectUseCase` (R146)
- [x] Wire `on_create_char` → `CharacterCreateUseCase` (R146)
- [x] Wire `on_delete_char` → `CharacterDeleteUseCase` (R146)
- [x] Wire `on_char_list` → `CharacterListUseCase` (R146)
- [x] Wire `on_char_list_110` → `CharacterListUseCase` (1.10+ reply format) (R146)
- [x] Wire `on_ladder` → `LadderQueryUseCase` (R146)
- [x] Wire `on_char_ladder` → `ILadderRepository::get_character_ladder_entry()` (R146)
- [x] Wire `on_create_game` → no-op stub (R146)
- [x] Wire `on_join_game` → no-op stub (R146)
- [x] Wire `on_cancel_create_game` → no-op stub (R146)
- [x] Wire `on_game_list` → no-op stub (R146)
- [x] Wire `on_game_info` → no-op stub (R146)
- [x] Wire `on_motd` → no-op stub (R146)
- [x] Wire `on_convert_char` → no-op stub (R146)
- [x] `src/v3/app/d2cs/CMakeLists.txt` — `app_d2cs` static library (R146)
- [x] `src/v3/CMakeLists.txt` updated — `add_subdirectory(app/d2cs)` (R146)
- [x] `tests/unit/app/d2cs/d2cs_session_handler_test.cpp` — 14 test cases, 40+ assertions (R146)
- [x] `tests/unit/app/d2cs/CMakeLists.txt` — test target registered (R146)
- [x] `tests/unit/app/CMakeLists.txt` updated — `add_subdirectory(d2cs)` (R146)
- [ ] Wire `on_login` → `AuthenticateWithBnetd` use case (future round)
- [ ] Wire `on_create_game` → `CreateGame` use case (future round)
- [ ] Wire `on_join_game` → `JoinGame` use case (future round)
- [ ] Wire `on_cancel_create_game` → `CancelCreateGame` use case (future round)
- [ ] Wire `on_game_list` → `GetGameList` use case (future round)
- [ ] Wire `on_game_info` → `GetGameInfo` use case (future round)
- [ ] Wire `on_motd` → `GetMotd` use case (future round)
- [ ] Wire `on_convert_char` → `ConvertCharacter` use case (future round)

#### 3b. D2DBS FSM Expansion ✅ Complete (R148)

The `D2DBSSessionFsm` stub existed in `src/v3/protocol/d2dbs/`. It has been
fully replaced to cover all `dbspacket.cpp` packet types.

Legacy source: `src/d2dbs/dbspacket.cpp` (900 LOC)

**Audit result (R148):** The legacy D2GS↔D2DBS protocol uses an **8-byte header**
(`uint16 size` + `uint16 type` + `uint32 seqno`, all LE) and type codes 0x30–0x34.
The original stub had completely wrong framing (2-byte header) and wrong type codes
(0x01–0x06 range) — both files were fully replaced.

D2DBS packet types (from `dbspacket.h` / `dbspacket.cpp`):
- `D2GS_D2DBS_SAVE_DATA_REQUEST` (0x30) — save character data
- `D2GS_D2DBS_GET_DATA_REQUEST` (0x31) — load character data
- `D2GS_D2DBS_UPDATE_LADDER` (0x32) — update ladder entry
- `D2GS_D2DBS_CHAR_LOCK` (0x33) — lock/unlock character
- `D2GS_D2DBS_ECHOREPLY` (0x34) — echo reply from D2GS

Checklist:
- [x] Audit legacy `dbspacket.cpp` / `dbspacket.h` for actual packet types and header format (R148)
- [x] Create `wire_types.hpp` with `PacketHeader`, type codes, data-type codes, result codes, and payload structs (R148)
- [x] Define `D2DBSPacketType` enum covering all 5 packet types (R148)
- [x] Implement `D2DBSSessionFsm::feed()` with 8-byte header reassembly buffer (R148)
- [x] Implement `D2DBSSessionFsm::dispatch()` routing to handlers (R148)
- [x] Implement each handler: `handle_save_data`, `handle_get_data`, `handle_update_ladder`, `handle_char_lock`, `handle_echo_reply` (R148)
- [x] Define `D2DBSFsmCallbacks` struct with one `std::function` per packet type (R148)
- [x] Implement reply builders: `make_save_data_reply`, `make_get_data_reply`, `make_echo_request` (R148)
- [x] `src/v3/protocol/d2dbs/include/protocol/d2dbs/wire_types.hpp` — created (R148)
- [x] `src/v3/protocol/d2dbs/include/protocol/d2dbs/fsm.hpp` — replaced stub (R148)
- [x] `src/v3/protocol/d2dbs/src/fsm.cpp` — replaced stub (R148)
- [x] `tests/unit/protocol/d2dbs/fsm_test.cpp` — 24 test cases covering all 5 packet types (R148)
- [x] `tests/unit/protocol/d2dbs/CMakeLists.txt` — test target registered (R148)
- [x] `src/v3/CMakeLists.txt` — no change needed (already had `protocol_d2dbs` inline) (R148)
- [x] `tests/unit/protocol/CMakeLists.txt` — no change needed (already had `d2dbs` entry) (R148)

#### 3c. D2DBS Domain Layer and Session Handler ✅ Complete (R149)

The D2DBS domain layer and application-layer session handler have been created,
parallel to the D2CS work done in R145–R146.

**Domain layer** (`src/v3/domain/d2dbs/`) — protocol-agnostic, persistence-agnostic:

Checklist:
- [x] `domain/d2dbs/types.hpp` — `CharacterSaveData`, `CharacterLockState`, `LadderUpdateEntry`, `GameResultData` (R149)
- [x] `domain/d2dbs/character_save_repository.hpp` — `ICharacterSaveRepository` interface: `load`, `save`, `lock`, `unlock`, `lock_state` (R149)
- [x] `domain/d2dbs/ladder_repository.hpp` — `ID2DBSLadderRepository` interface: `update_entry`, `find_entry` (R149)
- [x] `domain/d2dbs/use_cases.hpp` — `CharacterSaveUseCase`, `CharacterLoadUseCase`, `CharacterLockUseCase`, `CharacterUnlockUseCase`, `LadderUpdateUseCase` (R149)
- [x] `domain/d2dbs/in_memory_repositories.hpp` — `InMemoryCharacterSaveRepository` (composite key `"account\0char_name"`), `InMemoryD2DBSLadderRepository` (R149)
- [x] `domain/d2dbs/CMakeLists.txt` — `domain_d2dbs` INTERFACE library (R149)
- [x] `src/v3/CMakeLists.txt` updated — `add_subdirectory(domain/d2dbs)` (R149)
- [x] `tests/unit/domain/d2dbs/use_cases_test.cpp` — 12 test cases covering all 5 use cases (R149)
- [x] `tests/unit/domain/d2dbs/CMakeLists.txt` — test target `test_domain_d2dbs_use_cases` registered (R149)
- [x] `tests/unit/domain/CMakeLists.txt` updated — `add_subdirectory(d2dbs)` (R149)

**Application layer** (`src/v3/app/d2dbs/`) — bridges FSM callbacks to domain use cases:

Checklist:
- [x] `app/d2dbs/d2dbs_session_egress.hpp` — `ID2DBSSessionEgress` pure-virtual outbound port: `send_char_login_result`, `send_char_logout_result`, `send_char_save_result`, `send_char_load_result`, `send_ladder_update_result` (R149)
- [x] `app/d2dbs/d2dbs_session_handler.hpp` — `D2DBSSessionHandler` class with `make_callbacks()` factory (R149)
- [x] `app/d2dbs/src/d2dbs_session_handler.cpp` — wires all 6 `D2DBSFsmCallbacks` lambdas; combines `charexphigh`/`charexplow` into 64-bit experience; dispatches `on_char_lock` to lock/unlock based on `lockstatus` (R149)
- [x] `app/d2dbs/CMakeLists.txt` — `app_d2dbs` STATIC library (R149)
- [x] `src/v3/CMakeLists.txt` updated — `add_subdirectory(app/d2dbs)` (R149)
- [x] `tests/unit/app/d2dbs/d2dbs_session_handler_test.cpp` — 10 test cases (TC-01 through TC-10) covering save, load, lock, unlock, ladder, echo no-op (R149)
- [x] `tests/unit/app/d2dbs/CMakeLists.txt` — test target `test_app_d2dbs_session_handler` registered (R149)
- [x] `tests/unit/app/CMakeLists.txt` updated — `add_subdirectory(d2dbs)` (R149)

---

### Step 4 — Infrastructure: Persistence ❌ Not started

#### 4a. Character File Persistence (D2CS side)

The `FilesystemSaveStore` exists but covers only the raw `.d2s` binary blob.
`d2cs/d2charfile.cpp` (878 LOC) also manages:
- Character metadata (class, level, status flags) stored in a sidecar `.d2c` file
- Character list ordering stored in a `.d2l` file

Checklist:
- [ ] Audit `d2charfile.cpp` for all file formats written/read
- [ ] Extend `FilesystemSaveStore` to handle `.d2c` metadata sidecar
- [ ] Implement `FilesystemCharacterListStore` for `.d2l` ordering file
- [ ] Integration tests: round-trip save/load with real `.d2s` fixture files

#### 4b. Ladder Persistence

Both `d2cs/d2ladder.cpp` and `d2dbs/d2ladder.cpp` write ladder data to a binary
file (`d2ladder.dat`). The format is a fixed-size array of `t_d2ladder_entry` structs.

**Target:** `src/v3/infra/persistence/realm/include/infra/persistence/realm/filesystem_ladder_store.hpp`

Checklist:
- [ ] Define `LadderFileFormat` (binary layout matching legacy `t_d2ladder_entry`)
- [ ] Implement `FilesystemLadderStore` with `load()` / `save()` / `update_entry()`
- [ ] Implement `InMemoryLadderStore` for tests
- [ ] Unit tests: load/save/update/sort (target: 20+ tests)
- [ ] Integration test: read a real `d2ladder.dat` fixture

---

### Step 5 — Inter-Service Communication ❌ Not started

D2CS communicates with two external services:
1. **D2DBS** — character lock/unlock, save/load, ladder updates (TCP, port 6113)
2. **D2GS** — game server registration, game creation/join (TCP, port 6114)

Both connections are currently managed by raw socket code in `d2cs/s2s.cpp` (149 LOC),
`d2cs/d2gs.cpp` (453 LOC), and `d2cs/bnetd.cpp` (93 LOC).

#### 5a. D2CS ↔ D2DBS adapter

**Target:** `src/v3/app/d2cs/include/app/d2cs/d2dbs_client.hpp` + `.cpp`

Checklist:
- [ ] Define `ID2DBSClient` interface (lock, unlock, save, load, ladder_update)
- [ ] Implement `AsioD2DBSClient` using Asio TCP + `D2DBSSessionFsm`
- [ ] Implement `InMemoryD2DBSClient` for unit tests
- [ ] Unit tests: 5 operations × 3 scenarios = 15+ tests

#### 5b. D2CS ↔ D2GS adapter

**Target:** `src/v3/app/d2cs/include/app/d2cs/d2gs_client.hpp` + `.cpp`

Checklist:
- [ ] Define `ID2GSClient` interface (register_gs, create_game, join_game, list_games)
- [ ] Implement `AsioD2GSClient` using Asio TCP + `D2GSCodec`
- [ ] Implement `InMemoryD2GSClient` for unit tests
- [ ] Unit tests: 4 operations × 3 scenarios = 12+ tests

#### 5c. D2CS ↔ bnetd adapter

**Target:** `src/v3/app/d2cs/include/app/d2cs/bnetd_client.hpp` + `.cpp`

Checklist:
- [ ] Define `IBnetdClient` interface (authenticate_account, get_realm_info)
- [ ] Implement `LegacyBnetdClient` calling into `d2cs_legacy` bridge
- [ ] Unit tests: 2 operations × 3 scenarios = 6+ tests

---

### Step 4 — D2CS Composition Root (pvpgn_v3_d2cs binary) ✅ Complete (R147)

Wire the D2CS protocol stack with an Asio event loop to produce the
`pvpgn_v3_d2cs` standalone binary.

#### 4a. D2CSTcpSession

**Target:** `src/v3/app/d2cs/include/app/d2cs/d2cs_tcp_session.hpp`
           `src/v3/app/d2cs/src/d2cs_tcp_session.cpp`

Checklist:
- [x] `D2CSTcpSession` class owning `D2CSSessionFsm` + `D2CSSessionHandler` (R147)
- [x] Owns `InMemoryCharacterRepository` + `InMemoryLadderRepository` (placeholder) (R147)
- [x] Implements `ID2CSSessionEgress` — serialises responses via FSM packet builders (R147)
- [x] Implements Asio async read loop via `infra::net::TcpSession` callbacks (R147)
- [x] `send_realm_logon_result()` → `D2CSSessionFsm::make_login_reply()` (R147)
- [x] `send_char_list()` → `D2CSSessionFsm::make_char_list_reply()` (R147)
- [x] `send_char_select_result()` → `D2CSSessionFsm::make_char_login_reply()` (R147)
- [x] `send_char_create_result()` → `D2CSSessionFsm::make_create_char_reply()` (R147)
- [x] `send_char_delete_result()` → `D2CSSessionFsm::make_delete_char_reply()` (R147)
- [x] `send_ladder()` → stub 3-byte header (LADDERREPLY encoder deferred) (R147)

#### 4b. main.cpp

**Target:** `src/v3/app/d2cs/src/main.cpp`

Checklist:
- [x] `D2CSConfig` struct with defaults (port 6113, listen 0.0.0.0) (R147)
- [x] CLI argument parsing (--config, --port, --listen, --log-level, --threads) (R147)
- [x] `IoRuntime` (Asio thread pool) (R147)
- [x] `TcpListener` on port 6113 → `D2CSTcpSession` per accepted connection (R147)
- [x] SIGINT/SIGTERM → `IoRuntime::stop()` (R147)
- [x] Startup log: `"pvpgn_v3_d2cs listening on port 6113"` (R147)
- [x] Graceful shutdown: stop listener (R147)

#### 4c. CMakeLists.txt

**Target:** `src/v3/app/d2cs/CMakeLists.txt`

Checklist:
- [x] `pvpgn_v3_d2cs` executable target (R147)
- [x] Sources: `src/main.cpp` (R147)
- [x] `src/d2cs_tcp_session.cpp` added to `app_d2cs` library (R147)
- [x] Links: `app_d2cs`, `protocol_d2cs`, `domain_d2cs`, `infra_net`, `Boost::system`, `Threads` (R147)
- [x] Includes bnetd `TcpListener` header path (R147)
- [x] Install target (R147)
- [x] Guard: skipped when `PVPGN_V3_WITH_BOOST=OFF` (R147)

---

### Step 6 — Composition Roots 🔄 Partial (D2CS binary done; full wiring deferred)

Wire all layers together in the composition roots.

#### 6a. D2CS Composition Root (full wiring — future rounds)

**Target:** `src/v3/services/d2cs/src/d2cs_composition.cpp` (currently a stub)

Checklist:
- [ ] Instantiate `FilesystemSaveStore` with configured `savedir`
- [ ] Instantiate `FilesystemLadderStore` with configured `ladderdir`
- [ ] Instantiate `InMemoryCharacterRepository`
- [ ] Instantiate all application use-case services
- [ ] Instantiate `D2CSSessionHandler` wired to use cases
- [ ] Instantiate `AsioD2DBSClient` + `AsioD2GSClient` + `LegacyBnetdClient`
- [ ] Instantiate `AsioEventLoop` with configured listen address
- [ ] Wire `D2CSSessionFsm` → `D2CSSessionHandler` per accepted connection
- [ ] Load config from `d2cs.toml` via `infra/config/d2cs_config.hpp`
- [ ] Integration test: start composition root, connect a mock client, verify login flow

#### 6b. D2DBS Composition Root

**Target:** `src/v3/services/d2dbs/src/d2dbs_composition.cpp` (currently a stub)

Checklist:
- [ ] Instantiate `FilesystemSaveStore` with configured `chardir`
- [ ] Instantiate `FilesystemLadderStore` with configured `ladderdir`
- [ ] Instantiate `CharacterLock` (in-memory)
- [ ] Instantiate `DupeChecker`
- [ ] Instantiate all application use-case services
- [ ] Instantiate `D2DBSSessionFsm` wired to use cases
- [ ] Instantiate `AsioEventLoop` with configured listen address
- [ ] Load config from `d2dbs.toml` via `infra/config/d2dbs_config.hpp`
- [ ] Integration test: start composition root, connect a mock D2CS, verify echo + char lock

---

### Step 7 — Combined Mode Integration ❌ Not started

Run D2CS + D2DBS inside the bnetd process (single binary, no IPC overhead).

Checklist:
- [ ] Add `--with-d2cs` / `--with-d2dbs` flags to bnetd composition root
- [ ] When `--with-d2cs`: instantiate `D2CSComposition` on a separate Asio strand
- [ ] When `--with-d2dbs`: instantiate `D2DBSComposition` on a separate Asio strand
- [ ] Replace `AsioD2DBSClient` with `InProcessD2DBSClient` (direct function calls)
- [ ] Integration test: single-process bnetd + d2cs + d2dbs, full login + game create flow

---

## Legacy Handler Deletion Order

Once each step is complete, the corresponding legacy files can be deleted:

| Step | Files to Delete | Blocker |
|------|----------------|---------|
| After Step 3a | `src/d2cs/handle_d2cs.cpp` + `handle_d2cs.h` | D2CSSessionHandler wired |
| After Step 3a | `src/d2cs/handle_init.cpp` + `handle_init.h` | FSM handles init |
| After Step 3b | `src/d2dbs/dbspacket.cpp` + `dbspacket.h` | D2DBSSessionFsm complete |
| After Step 4a | `src/d2cs/d2charfile.cpp` + `d2charfile.h` | FilesystemSaveStore complete |
| After Step 4a | `src/d2cs/d2charlist.cpp` + `d2charlist.h` | FilesystemCharacterListStore complete |
| After Step 4b | `src/d2cs/d2ladder.cpp` + `d2ladder.h` | FilesystemLadderStore complete |
| After Step 4b | `src/d2dbs/d2ladder.cpp` + `d2ladder.h` | FilesystemLadderStore complete |
| After Step 4b | `src/d2dbs/dbsdupecheck.cpp` + `dbsdupecheck.h` | DupeChecker wired |
| After Step 5a | `src/d2cs/s2s.cpp` + `s2s.h` | AsioD2DBSClient complete |
| After Step 5b | `src/d2cs/d2gs.cpp` + `d2gs.h` | AsioD2GSClient complete |
| After Step 5b | `src/d2cs/handle_d2gs.cpp` + `handle_d2gs.h` | AsioD2GSClient complete |
| After Step 5b | `src/d2cs/game.cpp` + `game.h` | CreateGame/JoinGame use cases complete |
| After Step 5b | `src/d2cs/gamequeue.cpp` + `gamequeue.h` | GsQueue complete |
| After Step 5b | `src/d2cs/serverqueue.cpp` + `serverqueue.h` | GsQueue complete |
| After Step 5c | `src/d2cs/bnetd.cpp` + `bnetd.h` | LegacyBnetdClient complete |
| After Step 5c | `src/d2cs/handle_bnetd.cpp` + `handle_bnetd.h` | LegacyBnetdClient complete |
| After Step 6a | `src/d2cs/server.cpp` + `server.h` | D2CS composition root owns event loop |
| After Step 6a | `src/d2cs/net.cpp` + `net.h` | AsioEventLoop owns sockets |
| After Step 6a | `src/d2cs/connection.cpp` + `connection.h` | D2CSSessionHandler owns connections |
| After Step 6a | `src/d2cs/prefs.cpp` + `prefs.h` | d2cs.toml config complete |
| After Step 6a | `src/d2cs/cmdline.cpp` + `cmdline.h` | Composition root owns CLI |
| After Step 6a | `src/d2cs/handle_signal.cpp` + `handle_signal.h` | Composition root owns signals |
| After Step 6a | `src/d2cs/main.cpp` | Composition root is entry point |
| After Step 6a | `src/d2cs/setup.h`, `version.h`, `bit.h` | No longer needed |
| After Step 6b | `src/d2dbs/dbserver.cpp` + `dbserver.h` | D2DBS composition root owns event loop |
| After Step 6b | `src/d2dbs/charlock.cpp` + `charlock.h` | CharacterLock wired |
| After Step 6b | `src/d2dbs/prefs.cpp` + `prefs.h` | d2dbs.toml config complete |
| After Step 6b | `src/d2dbs/cmdline.cpp` + `cmdline.h` | Composition root owns CLI |
| After Step 6b | `src/d2dbs/handle_signal.cpp` + `handle_signal.h` | Composition root owns signals |
| After Step 6b | `src/d2dbs/main.cpp` | Composition root is entry point |
| After Step 6b | `src/d2dbs/setup.h`, `version.h` | No longer needed |

---

## Migration Order (Recommended)

```
R143  — This planning round (progress-master.md + phase4-d2-checklist.md)
R144  — Step 1a: D2Ladder domain object + unit tests
R145  — Step 1b: CharacterListRepository implementations + unit tests
R146  — Step 2a: D2CS use-case gaps (CreateGame, GetGameList, GetGameInfo, GetMotd)
R147  — Step 2a: D2CS use-case gaps (CancelCreateGame, ConvertCharacter, GetLadder, AuthBnetd)
R148  — Step 2b: D2DBS use-case gaps (EchoRequest, UpdateLadder, expand Save/Load)
R149  — Step 3a: D2CSSessionHandler wiring (13 callbacks) + tests
R150  — Step 3b: D2DBSSessionFsm expansion (6 packet types) + tests
R151  — Step 4a: Character file persistence (d2charfile + d2charlist) + tests
R152  — Step 4b: Ladder persistence (FilesystemLadderStore) + tests
R153  — Step 5a: AsioD2DBSClient + InMemoryD2DBSClient + tests
R154  — Step 5b: AsioD2GSClient + InMemoryD2GSClient + tests
R155  — Step 5c: LegacyBnetdClient + tests
R156  — Step 6a: D2CS composition root wired end-to-end + integration test
R157  — Step 6b: D2DBS composition root wired end-to-end + integration test
R158  — Delete d2cs legacy handlers (handle_d2cs, handle_init, handle_d2gs, handle_bnetd)
R159  — Delete d2cs domain files (d2charfile, d2charlist, d2ladder, game, gamequeue, serverqueue)
R160  — Delete d2cs infrastructure files (server, net, connection, prefs, cmdline, s2s, d2gs, bnetd, main)
R161  — Delete d2dbs legacy files (dbspacket, dbsdupecheck, charlock, d2ladder, dbserver, prefs, cmdline, main)
R162  — Step 7: Combined mode integration
```

---

## Complexity Ratings

| Step | Complexity | Reason |
|------|-----------|--------|
| Step 1a (D2Ladder domain) | Medium | Two legacy impls to reconcile; binary file format |
| Step 1b (CharacterListRepository) | Low | Small scope; pattern established |
| Step 2a (D2CS use cases) | Medium | 8 use cases; game lifecycle is complex |
| Step 2b (D2DBS use cases) | Low | 7 use cases; most are simple CRUD |
| Step 3a (D2CS FSM wiring) | Low | FSM complete; just wire callbacks |
| Step 3b (D2DBS FSM expansion) | Medium | 900-LOC legacy file; binary protocol |
| Step 4a (char file persistence) | Medium | Multiple file formats; `.d2s` + `.d2c` + `.d2l` |
| Step 4b (ladder persistence) | Low | Fixed binary format; well-understood |
| Step 5a (D2CS↔D2DBS adapter) | Medium | Async TCP; reconnect logic |
| Step 5b (D2CS↔D2GS adapter) | Medium | Async TCP; game server registration |
| Step 5c (D2CS↔bnetd adapter) | Low | Legacy bridge pattern established |
| Step 6a (D2CS composition) | High | All layers must be wired; config migration |
| Step 6b (D2DBS composition) | Medium | Simpler than D2CS; fewer dependencies |
| Step 7 (combined mode) | High | Cross-service IPC elimination; integration testing |

---

## Cross-References

- [`plans/refactoring-plan-legacy-d2.md`](plans/refactoring-plan-legacy-d2.md) — original 7-step plan
- [`plans/phase3-bnetd-checklist.md`](plans/phase3-bnetd-checklist.md) — Phase 3 (bnetd) checklist
- [`plans/progress-master.md`](plans/progress-master.md) — master progress tracker
- [`src/v3/protocol/d2cs/include/protocol/d2cs/fsm.hpp`](../src/v3/protocol/d2cs/include/protocol/d2cs/fsm.hpp) — D2CS FSM header (310 lines)
- [`src/v3/protocol/d2cs/src/fsm.cpp`](../src/v3/protocol/d2cs/src/fsm.cpp) — D2CS FSM impl (706 lines)
- [`src/d2cs/CMakeLists.txt`](../src/d2cs/CMakeLists.txt) — d2cs_legacy carve-out
- [`src/d2dbs/CMakeLists.txt`](../src/d2dbs/CMakeLists.txt) — d2dbs_legacy carve-out
- [`conf/d2cs.toml.in`](../conf/d2cs.toml.in) — D2CS TOML config template
- [`conf/d2dbs.toml.in`](../conf/d2dbs.toml.in) — D2DBS TOML config template
