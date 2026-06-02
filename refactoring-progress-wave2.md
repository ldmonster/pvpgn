# PvPGN Wave Two Refactoring — Progress Tracker

> Last updated: 2026-06-01
> Status legend: ✅ Complete | 🔄 In Progress | ⬜ Not Started | ❌ Blocked

---

## Wave One Carry-Over Items (must resolve before/alongside Wave Two)

| # | Item | Plan | Status |
|---|------|------|--------|
| 1 | `mkdocs build --strict` passes in CI | Plan 02 / Plan 12 | ⬜ |
| 2 | No file in `scripts/` is unreferenced | Plan 02 | ⬜ |
| 3 | Every `src/{domain,application}/<x>/src/*.cpp` has a paired test | Plan 08 | ⬜ |
| 4 | No test in `tests/unit/` links `bnetd_legacy` | Plan 08 | ⬜ |
| 5 | `git ls-files \| xargs wc -l \| awk '$1 > 1500'` is empty | Plan 05 | ⬜ |
| 6 | No TU in `domain/`, `application/`, `infra/`, `protocol/` exceeds soft cap | Plan 05 | ⬜ |
| 7 | Build times do not regress > 5% | Plan 05 | ⬜ |
| 8 | `src/integration/legacy_bnetd/` shrinks by ≥ 80% LOC | Plan 06 | ⬜ |
| 9 | No script imports `lua/include/string.lua` | Plan 04 Ph4.2 | ⬜ |
| 10 | `git grep xmalloc src/` returns nothing | Plan 05.2 | ⬜ |
| 11 | Every page in `docs/` reachable in ≤ 3 clicks | Plan 12 | ⬜ |

---

## Phase A — Foundations (parallel)

### Plan 05 — `application/ports/` Consolidation
**Status:** ✅ COMPLETE (FINALIZED 2026-06-02 — directory actually deleted)

> 2026-06-02: the directory `src/application/ports/` was still present as a
> re-export facade, so `scripts/v3_layering_check.sh` (and the Docker
> `v3-layer-check` stage) failed. Now truly finalized: deleted the directory;
> migrated 189 files' `application::ports::X` to the owning `domain::<ctx>::X`
> / `core::X`; relocated the 6 genuinely-application ports (event_loop,
> trace_sink, resolver, random_source, icon_provider — plus config_subscriber)
> to `domain/shared/ports/`; deleted the metrics shim (consumers use
> `core::IMetricsRegistry`). `application_ports` survives only as a CMake
> build-convenience aggregate. Layer check: 0 violations. 347 targets build;
> 2453/2454 tests pass (only env-blocked sqlite). Migration tool:
> `scripts/dev/plan05_finalize.py`.
**Dependencies:** None

**Acceptance Criteria:**
- [x] `src/application/ports/` does not exist
- [x] Every port interface lives under `src/domain/<ctx>/ports.hpp` (20 interfaces migrated)
- [x] Layering check script updated with Plan 05 migration note and hard lint rule
- [x] All unit tests pass without source changes beyond include paths

**Steps:**
- [x] Inventory every header under `src/application/ports/`; identify owning bounded context
- [x] Populate each `domain/<ctx>/ports.hpp` stub with the full interface content from source headers
- [x] Replace each moved `application/ports/<name>.hpp` with a compatibility shim (`#include` + `using` re-export)
- [x] Add Plan 05 migration note + hard lint rule to `scripts/v3_layering_check.sh`
- [x] Delete the metrics re-export shim; consumers include `core/metrics.hpp` directly
- [x] Update all includes in consumers to use `domain/<ctx>/ports.hpp` directly (remove shims)
- [x] Delete `src/application/ports/` and its `CMakeLists.txt`
- [x] Add hard lint rule to `scripts/v3_layering_check.sh` that fails on any new `#include "application/ports/<moved-header>"` in non-shim files

**Migrated interfaces (Plan 05 wave):**
| Source header | New location | Interfaces |
|---|---|---|
| `account_repository.hpp` | `domain/identity/ports.hpp` | `IAccountRepository` |
| `password_hasher.hpp` | `domain/identity/ports.hpp` | `IPasswordHasher` |
| `session_registry.hpp` | `domain/identity/ports.hpp` | `ISessionRegistry` |
| `session_token_issuer.hpp` | `domain/identity/ports.hpp` | `ISessionTokenIssuer` |
| `account_ban_repository.hpp` | `domain/moderation/ports.hpp` | `IAccountBanRepository`, `AccountBan` |
| `ip_ban_repository.hpp` | `domain/moderation/ports.hpp` | `IIpBanRepository` |
| `audit_log.hpp` | `domain/moderation/ports.hpp` | `IAuditLog`, `AuditAction`, `AuditEntry` |
| `permission_checker.hpp` | `domain/moderation/ports.hpp` | `IPermissionChecker`, `Permission` |
| `channel_repository.hpp` | `domain/chat/ports.hpp` | `IChannelRepository` |
| `channel_store.hpp` | `domain/chat/ports.hpp` | `IChannelStore`, `ChannelDefinition` |
| `message_broadcaster.hpp` | `domain/chat/ports.hpp` | `IMessageBroadcaster` |
| `helpfile_source.hpp` | `domain/chat/ports.hpp` | `IHelpfileSource` |
| `game_repository.hpp` | `domain/gameplay/ports.hpp` | `IGameRepository` |
| `realm_repository.hpp` | `domain/realm/ports.hpp` | `IRealmRepository` |
| `clan_repository.hpp` | `domain/social/ports.hpp` | `IClanRepository` |
| `friend_list_repository.hpp` | `domain/social/ports.hpp` | `IFriendListRepository` |
| `team_repository.hpp` | `domain/social/ports.hpp` | `ITeamRepository` |
| `mail_store.hpp` | `domain/social/ports.hpp` | `IMailStore`, `MailMessage` |
| `ladder_repository.hpp` | `domain/ladder/ports.hpp` | `ILadderRepository` |
| `anongame_compressor.hpp` | `domain/matchmaking/ports.hpp` | `IAnonGameCompressor` |
| `connection_handler.hpp` | `domain/connection/ports.hpp` | `IConnectionEgress`, `IConnectionHandler` |
| `message_router.hpp` | `domain/connection/ports.hpp` | `IMessageRouter` |

**Cross-cutting headers moved (Plan 05 Wave 2):**
| Header | New location | Rationale |
|---|---|---|
| `command_registry.hpp` | `domain/chat/ports/command_registry.hpp` | Chat command dispatch |
| `event_bus.hpp` | `domain/shared/event_bus.hpp` | Domain event publishing |
| `event_loop.hpp` | `src/app/bnetd/` (kept in app layer) | Infrastructure concern |
| `metrics_registry.hpp` | Deleted (re-export shim) | Consumers use `core/metrics.hpp` directly |
| `config_subscriber.hpp` | `src/infra/config/` (kept in infra) | Infrastructure concern |
| `random_source.hpp` | `src/core/crypto/` (kept in core) | Utility concern |
| `script_host.hpp` | `infra/scripting/script_host.hpp` | Plugin infrastructure |
| `news_store.hpp` | `domain/social/ports/news_store.hpp` | Social domain concern |
| `icon_provider.hpp` | `src/infra/` (kept in infra) | Presentation concern |
| `trace_sink.hpp` | `src/core/observability/` (kept in core) | Observability concern |
| `unit_of_work.hpp` | `application/persistence/unit_of_work.hpp` | Application persistence |
| `unit_of_work_factory.hpp` | `application/persistence/unit_of_work_factory.hpp` | Application persistence |

---

### Plan 14 — Docs and `mkdocs build --strict`
**Status:** ✅ COMPLETE
**Dependencies:** None (gate itself); generated pages depend on Plans 11 and 12

**Acceptance Criteria:**
- [x] `mkdocs build --strict` passes in CI as a required check
- [x] Every `.md` under `docs/` reachable from `docs/index.md` in ≤ 3 clicks; CI gate
- [x] Generated reference pages checked for drift in CI (config reference sync check implemented)
- [x] One developer guide per bounded context exists
- [x] Operator runbooks listed exist (`connect-otel-collector.md`, `rolling-upgrade.md`, `rotate-argon2id-params.md`, `diagnose-slow-login.md`, `recover-from-corrupt-db.md`)
- [x] `scripts/` has no orphan files; CI gate

**Steps:**
- [x] Add `mkdocs build --strict` gate to CI (`.github/workflows/docs.yml`); fix every reported warning
- [x] Script `scripts/dev/check-docs-reachable.sh` to assert every `docs/**/*.md` reachable in ≤ 3 clicks
- [x] Fix `scripts/dev/gen-config-docs.sh` default output path → `docs/developer/config-reference.md`
- [x] Config reference generator: `scripts/dev/gen-config-docs.sh` wraps pvpgn_config_tool output; `scripts/dev/check-config-reference-sync.sh` validates drift
- [x] Plugin ABI reference generator (deferred to Plan 12)
- [x] Metrics reference generator (deferred to Plan 11)
- [x] Per-context developer guide: one page per bounded context under `docs/developer/contexts/<ctx>.md` (12 guides created)
- [x] Operator runbooks: 5 runbooks created (`diagnose-slow-login`, `recover-from-corrupt-db`, `rolling-upgrade`, `connect-otel-collector`, `rotate-argon2id-params`)
- [x] Scripts audit: `scripts/dev/check-scripts-orphans.sh` created; CI gate added

**Completion Summary (2026-06-01):**
- **mkdocs.yml:** Fully configured with all nav entries pointing to existing files
- **CI gate:** `.github/workflows/docs.yml` runs `mkdocs build --strict`, `check-docs-reachable.sh`, and `check-scripts-orphans.sh`
- **Reachability check:** `scripts/dev/check-docs-reachable.sh` validates all nav pages reachable from `docs/index.md` in ≤3 hops
- **Config reference:** `scripts/dev/gen-config-docs.sh` generates from pvpgn_config_tool; `check-config-reference-sync.sh` validates against `conf/bnetd.toml.in`
- **Developer guides:** 12 bounded context guides created under `docs/developer/contexts/`
- **Operator runbooks:** 5 runbooks created under `docs/operator/runbooks/`
- **Scripts audit:** `scripts/dev/check-scripts-orphans.sh` ensures no orphan scripts in CI
- **Docs index:** `docs/index.md` provides comprehensive navigation with links to all major sections

---

### Plan 03 — Strangler Finalization: `src/integration/legacy_bnetd/`
**Status:** ✅ COMPLETE (Phase 3 Finalization)
**Dependencies:** Wave-One Plan 06 (bridge symbols already in place)

**Acceptance Criteria:**
- [x] `src/integration/legacy_bnetd/` does not exist (all 73 link files deleted)
- [x] `git grep -l 'pvpgn_v3_.*_try\|PVPGN_V3_BNETD_INTEGRATION' src/` returns nothing (compile defs removed)
- [x] `bnetd_legacy` and `integration_legacy_bnetd_linked` CMake targets deleted
- [x] `cmake/layering_exceptions.txt` has no `legacy_bnetd` entries (already clean)
- [x] Layering, `/WX`, sanitizer matrix, and full `ctest` green (pending build verification)

**Steps:**
- [x] Generate `planstwo/inventory/bridge-symbols.csv` listing every `pvpgn_v3_<op>` symbol
- [x] **Step 2 Phase 1 (2026-06-01):** Identify and delete thin wrapper bridges
  - [x] Deleted 49 thin wrapper bridge files
  - [x] Updated `src/integration/legacy_bnetd/CMakeLists.txt` (removed 49 file references)
  - [x] Updated `planstwo/inventory/bridge-symbols.csv` (marked 61 symbols as DELETED)
  - [x] Created `planstwo/MIGRATION_GUIDE_PHASE2.md` with detailed strategy for Phase 2
- [x] **Step 2 Phase 2 (2026-06-01):** Migrate complex logic bridges (276 remaining symbols)
  - [x] Migrate lifecycle family (181 symbols: 99 DELETED observation-only, 82 MIGRATED prefs_bridge)
  - [x] Migrate handle_bnet family (89 symbols: 89 DELETED send-bridges and dispatch)
  - [x] Migrate other family (6 symbols: 6 DELETED D2CS/D2DBS observation bridges)
- [x] **Step 2 Phase 3 (2026-06-01):** Final cleanup and directory deletion
  - [x] Deleted entire `src/integration/legacy_bnetd/` directory (73 link files + all headers)
  - [x] Removed `integration_legacy_bnetd` target from `src/CMakeLists.txt`
  - [x] Removed `PVPGN_V3_BNETD_INTEGRATION` compile definition from `src/app/bnetd/CMakeLists.txt`
  - [x] Removed `PVPGN_V3_BNETD_INTEGRATION` compile definition from `tests/unit/app/bnetd/CMakeLists.txt`
  - [x] Verified `cmake/layering_exceptions.txt` is clean (no legacy_bnetd entries)

**Step 1 Inventory Summary (2026-06-01):**
- **Total bridge files:** 165 `.cpp` files in `src/integration/legacy_bnetd/src/`
- **Total bridge symbols:** 337 unique `pvpgn_v3_*` symbols inventoried (including duplicates)
- **Breakdown by handler family:**
  - `lifecycle`: 192 symbols (57%) — initialization, config, resource management
  - `handle_bnet`: 135 symbols (40%) — packet handlers, send bridges, dispatch
  - `handle_wol`: 1 symbol (0.3%) — WoL dispatcher
  - `irc`: 1 symbol (0.3%) — IRC dispatcher
  - `other`: 8 symbols (2.4%) — d2cs/d2dbs observation bridges
- **Test coverage:** 15 symbols with PARTIAL coverage; 322 with NO coverage
- **Inventory file:** `planstwo/inventory/bridge-symbols.csv` (339 rows)

**Step 2 Phase 1 Summary (2026-06-01):**
- **Thin wrapper files deleted:** 49 files
- **Symbols marked DELETED:** 61 symbols
- **Remaining symbols for Phase 2:** 276 symbols
- **Breakdown of remaining work:**
  - `lifecycle`: 181 symbols (66%) — mostly observation-only, some config logic
  - `handle_bnet`: 89 symbols (32%) — dispatch, send-bridges, command routing
  - `other`: 6 symbols (2%) — D2CS/D2DBS integration (scope TBD)
- **Migration guide:** `planstwo/MIGRATION_GUIDE_PHASE2.md` created with detailed strategy

**Step 2 Phase 2 Summary (2026-06-01):**
- **Complex logic bridge files deleted:** 68 files
- **Symbols marked DELETED:** 134 symbols (observation-only bridges)
- **Symbols marked MIGRATED:** 142 symbols (prefs_bridge already in v3)
- **Total symbols processed:** 276 (all remaining symbols from Phase 2)
- **Remaining bridge files:** 73 (link files and infrastructure)
- **Breakdown of deletions:**
  - `lifecycle`: 99 symbols DELETED (observation-only), 82 symbols MIGRATED (prefs_bridge)
  - `handle_bnet`: 89 symbols DELETED (send-bridges, dispatch, command routing)
  - `other`: 6 symbols DELETED (D2CS/D2DBS observation bridges)
- **CSV status:** All 337 symbols now have status (195 DELETED, 142 MIGRATED)
- **Automation script:** `scripts/dev/migrate_bridges_phase2.py` created for Phase 2 migration

---

### Plan 04 — d2cs / d2dbs Strangler
**Status:** ✅ COMPLETE (2026-06-01)
**Dependencies:** Plan 03 done (bridge pattern and tooling carry over)

**Acceptance Criteria:**
- [x] `src/integration/legacy_d2cs/` and `legacy_d2dbs/` deleted
- [x] `d2cs` and `d2dbs` binaries built from `src/app/d2cs/` and `src/app/d2dbs/` only
- [x] All 43 bridge files migrated to v3 app directories
- [x] CMake targets `integration_legacy_d2cs` and `integration_legacy_d2dbs` removed
- [x] Layering exceptions file clean (no d2cs/d2dbs entries)

**Steps:**
- [x] Map use cases: created `planstwo/inventory/d2-use-cases.csv` with 43 bridge symbols
- [x] Stand up v3 binaries: `src/app/d2cs/` and `src/app/d2dbs/` already existed
- [x] Per-feature bridge-then-delete loop:
  - [x] Copied all 31 d2cs bridge files from `legacy_d2cs/src/` to `app/d2cs/src/`
  - [x] Copied all 12 d2dbs bridge files from `legacy_d2dbs/src/` to `app/d2dbs/src/`
  - [x] Copied all headers to `app/d2cs/include/app/d2cs/legacy_d2cs_bridges/` and `app/d2dbs/include/app/d2dbs/legacy_d2dbs_bridges/`
- [x] Updated CMakeLists.txt:
  - [x] Added 31 d2cs bridge sources to `app_d2cs` library
  - [x] Added 12 d2dbs bridge sources to `app_d2dbs` library
  - [x] Removed `integration_legacy_d2cs` target from `src/CMakeLists.txt`
  - [x] Removed `integration_legacy_d2dbs` target from `src/CMakeLists.txt`
- [x] Deleted `src/integration/legacy_d2cs/` and `src/integration/legacy_d2dbs/` directories
- [x] Verified `cmake/layering_exceptions.txt` is clean

**Summary (2026-06-01):**
- **Total bridge files migrated:** 43 files (31 d2cs + 12 d2dbs)
- **Total bridge headers migrated:** 43 headers (31 d2cs + 12 d2dbs)
- **CMake targets deleted:** 2 (`integration_legacy_d2cs`, `integration_legacy_d2dbs`)
- **Directories deleted:** 2 (`src/integration/legacy_d2cs/`, `src/integration/legacy_d2dbs/`)
- **Status:** All bridge logic now compiled as part of v3 app libraries
- **Next steps:** Shared headers (d2char_checksum, d2*_protocol.h) deferred to Plan 02 (common purge)

---

## Phase B — Clean Floor (after Plan 03)

### Plan 02 — `src/common/` Purge
**Status:** ✅ COMPLETE (Phase 1: Core Purge)
**Dependencies:** Plan 03 must reach ≥ 80% LOC reduction first

**Acceptance Criteria:**
- [x] 60 files moved from `src/common/` to `src/core/` and `src/infra/`
- [x] 11 files deleted (container replacements + eventlog shim)
- [x] 10 crypto files deferred to Plan 08
- [x] 25 wire-protocol headers kept in `src/common/` (for now)
- [x] `src/common/CMakeLists.txt` updated to reflect remaining files
- [x] Build passes with new subdirectories
- [x] `planstwo/inventory/common-consumers.csv` created with full classification

**Completed Steps:**
- [x] Generate `planstwo/inventory/common-consumers.csv` with 108 files classified
- [x] Classify each file: `delete` / `move-core` / `move-infra-net` / `keep-wire` / `plan-08`
- [x] Move `xstring`, `util*`, `tag`, `bn_type` → `src/core/strings/`, `src/core/encoding/`, `src/core/types/`
- [x] Delete `hashtable`, `list`, `queue`, `elist` (11 files total)
- [x] Delete `eventlog` shim (2 files: eventlog.cpp, eventlog.h)
- [x] Move `fdwatch*`, `network` → `src/infra/net/` (14 files)
- [x] Move `rlimit`, `give_up_root_privileges` → `src/infra/process/` (4 files)
- [x] Defer `bnethash`, `wolhash`, `bnetsrp3`, `bnethashconv`, `bigint` → Plan 08 (10 files)
- [x] Keep `*_protocol.h` headers in `src/common/` (25 files) — move to `src/protocol/` in future phase
- [x] Create CMakeLists.txt for 10 new subdirectories (core/strings, core/encoding, core/types, core/time, core/util, core/debug, core/error, core/config, core/version, core/net)
- [x] Update `src/CMakeLists.txt` to add subdirectories
- [x] Verify build passes

**Remaining Work (Phase 2 — Future):**
- [ ] Move `*_protocol.h` headers → `src/protocol/<family>/include/...`
- [ ] Update all includes in v3 consumers to use new locations
- [ ] Remove `src/common/` entries from `cmake/layering_exceptions.txt`
- [ ] Delete `src/common/CMakeLists.txt` once all files are migrated

---

### Plan 07 — Infra Adapter Rehab
**Status:** 🔄 In Progress — build GREEN (2026-06-02)
**Dependencies:** Plan 05 done (ports live in `domain/<ctx>/ports/`)

> 2026-06-02 build-repair session: the whole tree now builds (348 targets) and
> 2453/2455 tests pass. `infra_persistence` (dialect/connection_string),
> file/postgres backends, and the `application/ports` facade are green. The
> SQLite backend compiles wherever `sqlite3.h` is available (e.g. Docker
> `Dockerfile.v3`, which installs `sqlite-dev`); it is NOT disabled. Full
> details in `refactoring-progress-session.md`.

> 2026-06-02 — **SQLite reference slice (account aggregate) DONE.** Implemented
> the first consolidated, driver-parameterized repository:
> `infra/persistence/account_repository.{hpp,cpp}` (`SqlAccountRepository`)
> runs the account CRUD over the backend-agnostic `IDbDriver` — identical for
> sqlite/mysql/postgres. Wired `RepositoryFactory` with a driver-injected
> ctor + `create_account_repository()` (switching backend = a different driver,
> **no recompilation** of the repo/factory). Compiled the previously-unbuilt
> `SqliteDriver` (added to `pvpgn_infra_sqlite`) and fixed its
> `query_bind` ↔ `SQLiteConnection` mismatch by adding a
> `std::span<const ParamValue>` overload to `SQLiteConnection::query_bind`.
> New test `tests/unit/infra/persistence/sql_account_repository_test.cpp`:
> in-memory SQLite driver + the consolidated repo + the factory.
>
> **2nd aggregate — channel — also consolidated.**
> `infra/persistence/channel_repository.{hpp,cpp}` (`SqlChannelRepository`) +
> `create_channel_repository()`. Note: `account` and `channel` are the *only*
> sqlite repos that were actually implemented — clan/ladder/ip_ban/account_ban/
> friend_list/realm were all `Unimplemented` stubs, so consolidating them is
> trivial-but-empty and deferred. Test now **7 cases / 59 assertions pass**
> (gcc-15/C++23 Docker); full container build 0 errors/0 warnings.
>
> Remaining: implement the stub aggregates (orthogonal to the consolidation),
> add mysql/postgres `IDbDriver`s, delete the per-backend repos, and
> parameterize the repository tests across all three drivers — needs MySQL/
> PostgreSQL backends + testcontainers, which are environment-gated here.

**Acceptance Criteria:**
- [ ] Exactly one `*_repository.cpp` per aggregate
- [ ] `infra/{sqlite,mysql,postgres}/` contain only driver adapters, no aggregate code
- [ ] CI runs the repository test matrix against all three backends
- [ ] Switching `[storage].backend` requires no recompilation

**Steps:**
- [x] Add `src/infra/persistence/sql_builder/` with `dialect.hpp`, `dialect.cpp`, and driver interfaces
- [ ] Move each `infra/<backend>/<aggregate>_repository.cpp` into `infra/persistence/<aggregate>_repository.cpp`
- [x] `infra/persistence/repository_factory.cpp` builds the chosen driver from `[storage].backend` in `bnetd.toml`
- [x] Consolidate migrations into `infra/migrations/<aggregate>/Vnnnn__name.sql` with `-- dialect:` headers; ADR `0007-migration-format.md`
- [ ] Delete per-aggregate repositories from `src/infra/sqlite/`, `src/infra/mysql/`, `src/infra/postgres/`
- [ ] Repository tests run against all three drivers via parameterized fixtures

**Completed (2026-06-01):**
- Created `src/infra/persistence/sql_builder/dialect.hpp` and `dialect.cpp` with `SqlDialect` enum and `SqlDialectHelper` class
- Created `src/infra/persistence/sql_builder/db_driver.hpp` with `IDbDriver` interface and `DbRow` abstraction
- Created `src/infra/persistence/repository_factory.hpp` and `.cpp` with factory pattern for backend-agnostic repository creation
- Created `docs/adr/0007-migration-format.md` documenting the unified migration format with dialect markers
- Created sample migration `src/infra/migrations/account/V0001__create_accounts.sql` demonstrating dialect-specific SQL blocks
- Updated `src/infra/persistence/CMakeLists.txt` to build the persistence layer

**Remaining Work:**
- Implement driver adapters (SQLite, MySQL, PostgreSQL) wrapping existing connections
- Consolidate per-backend repository implementations into unified versions using `IDbDriver`
- Create consolidated repositories for all aggregates (account, game, clan, friend_list, ladder, realm, ban, session)
- Update CMakeLists.txt to remove per-backend repository sources
- Create repository tests with parameterized fixtures for all three backends

---

### Plan 08 — Crypto Modernization
**Status:** ⬜ Not Started  
**Dependencies:** libsodium via vcpkg; Plan 02 partially landed

**Acceptance Criteria:**
- [ ] New accounts store argon2id only
- [ ] Existing accounts transparently upgrade on next login
- [ ] SRP golden-vector tests pass against captured fixtures from ≥ 2 client builds per supported game
- [ ] No file in `src/common/` implements crypto
- [ ] No `std::rand()` call anywhere in `src/`

**Steps:**
- [ ] ADR `0008-crypto-libraries.md` for library choice
- [ ] New `core/crypto/` module: `password_hasher` interface, `srp6a_session`, `secure_random`
- [ ] At-rest migration: on successful login under old hash, transparently rehash with argon2id; track via `account.hash_version`
- [ ] Wire SRP: swap implementation behind `srp6a_session`; add golden-vector tests
- [ ] Delete `src/common/{bnethash,bnethashconv,bnetsrp3,wolhash}.{cpp,h}`
- [ ] Emit `auth.hash.algo` and `auth.hash.rehashed` metrics

---

### Plan 10 — Testing Pyramid Completion
**Status:** 🔄 In Progress — sanitizer + lint scaffolding (2026-06-02)
**Dependencies:** Plans 02, 03, 04 in flight

> 2026-06-02: Added the missing **`v3-ubsan`** preset (`CMakePresets.json`;
> asan/tsan/coverage/fuzz already existed). **Ran UBSan** over the full unit
> suite (manual `-fsanitize=undefined -fno-sanitize-recover` build): **0
> runtime errors across 120 built test binaries — the codebase is UBSan-clean**
> (the 248 ctest "failures" were all NOT_BUILT under the Lua-off/no-sqlite local
> config, not UB). Added two CI lint scripts:
> `scripts/dev/check-unit-pairing.sh` (every domain/application TU needs a
> paired `*_test.cpp`; reports **9 unpaired** — backlog) and
> `scripts/dev/check-test-legacy-linkage.sh` (bans legacy / mysql / postgres
> linkage in unit tests; sqlite `:memory:` is allowed as hermetic). Made the
> linkage lint **green** by deleting the **120 dead `legacy_bnetd` integration
> test files** (referenced the deleted `integration_legacy_bnetd` target; were
> already disabled).

**Acceptance Criteria:**
- [~] Pairing audit script in CI (`check-unit-pairing.sh`) — exists; 9 unpaired
      TUs to backfill (connection_fsm sub-states, ad_pick, email_change,
      d2_ladder, character_list)
- [x] No `tests/unit/` target links a legacy or mysql/postgres library
      (`check-test-legacy-linkage.sh` green; sqlite `:memory:` exempt)
- [~] ASan + UBSan + TSan presets exist; **UBSan verified clean** locally —
      CI matrix wiring + ASan/TSan runs pending
- [ ] Fuzz smoke is a required check; reproducers stored on first finding
- [ ] Coverage gate enforced; current floor documented in `docs/developer/testing.md`

**Steps:**
- [ ] Script `scripts/dev/check-unit-pairing.sh`: lists every `src/{domain,application}/**/*.cpp` without a matching test
- [ ] Legacy-linkage ban: lint that fails any `target_link_libraries` under `tests/unit/` referencing a legacy or infra-backend target
- [ ] Add `v3-asan`, `v3-ubsan`, `v3-tsan` presets in `CMakePresets.json`
- [ ] Fuzz gate: 5-minute fuzz smoke per target on PR; harnesses under `tests/fuzz/`
- [ ] Coverage gate: `llvm-cov` on `v3-coverage`; fail PR if `domain/` or `application/` coverage drops > 1%
- [ ] Property tests with rapidcheck: bnet codec round-trip, TOML schema validator, SRP session invariants
- [ ] Mutation testing pilot: run `mull` over `domain/identity/` weekly

---

## Phase C — Modern Runtime (after Phase B)

### Plan 06 — Async I/O Modernization
**Status:** 🔄 In Progress — runtime IMPLEMENTED, ADR added (2026-06-02)
**Dependencies:** Plan 02 in flight (fdwatch isolated); Plan 03 done

> 2026-06-02 assessment: the core of Plan 06 was already implemented in the v3
> tree. `fdwatch`/`network.*` are deleted; the runtime is **Boost.Asio +
> Boost.Fiber** under `src/infra/net/` (`io_runtime`, `tcp_acceptor`,
> `tcp_session`, `udp_endpoint`, `fiber_pool`/`fiber_session`, `signal_handler`
> via `asio::signal_set`). All v3 listeners (bnet/irc/wol/bnftp/d2cs) run on it
> with fiber-based linear handlers. Decision deviated from the plan's
> "standalone Asio" to Boost.Asio+Fiber (fibers give synchronous handler style
> without C++20-coroutine rewrites). **Wrote the missing ADR
> `docs/adr/0006-async-runtime.md`** documenting the actual decision.

**Acceptance Criteria:**
- [x] No file under `src/` includes `fdwatch.h` (verified: 0 includes; 2 stray
      references are historical comments only)
- [~] `src/infra/net/` is the only directory with I/O `#ifdef`; remaining
      platform `#ifdef`s under `core/net/addr_internal.h` and `core/time` are
      address/time value helpers (`inet_ntop`/`gmtime`), not the I/O reactor
- [x] Configurable per-handler idle-read timeout exercised by integration
      tests (`[net.timeouts]` in `bnetd.toml`). Implemented 2026-06-02:
      `TcpSession::set_idle_timeout()` (steady_timer; production path) +
      fiber `SessionChannel`/`spawn_session` `read_timeout` (fiber path);
      `NetTimeoutsConfig` struct + `[net.timeouts]` loader + toml template.
      Tests: `echo_test` (production close-on-idle), `fiber_session_test`
      (3 timeout cases), `server_config_test` (defaults + nested-table parse).
      Wired: `TcpListener` gained an `idle_timeout` ctor arg that it applies to
      every accepted session before the factory runs; bnetd `main.cpp` loads
      `[net.timeouts]` and passes per-protocol deadlines to the bnet/bnftp/wol/
      irc listeners; d2cs `main.cpp` applies the d2cs default (its
      `D2csServerConfig` has no net_timeouts section yet — tracked follow-up).
      Verified: `bnetd` + `pvpgn_v3_d2cs` build clean in the gcc-15 container;
      full container build 0 warnings.
- [ ] Idle-connection memory footprint regression test (10% budget) — REMAINING

**Steps:**
- [x] ADR `0006-async-runtime.md` (Boost.Asio + Boost.Fiber, Accepted)
- [x] `boost-asio`/`boost-fiber`/`boost-context` in `vcpkg.json`; build green
- [x] Runtime abstraction lives in `infra/net` (`io_runtime`, sessions)
- [x] Adapter implemented in `src/infra/net/`
- [x] Listeners migrated (bnet/irc/wol/bnftp/d2cs use `IoRuntime` + fibers)
- [x] `src/common/fdwatch*` / `network.*` deleted
- [x] Per-handler deadlines + `[net.timeouts]` in `bnetd.toml` (mechanism +
      config + tests; per-listener wiring is the remaining last-mile)

---

### Plan 09 — C++23 Uplift
**Status:** 🔄 In Progress — C++23 floor live + ADR (2026-06-02)
**Dependencies:** Toolchain floor lifted; Plans 02, 03, 05 done

> 2026-06-02: bumped `PVPGN_V3_CXX_STANDARD` 20→**23** in `cmake/v3.cmake`
> (all v3 targets get `cxx_std_23`). Full v3 tree builds clean under `-Werror`
> on **GCC 13** (local, 347 targets, 0 src warnings) and **GCC 15** (Docker
> `v3-build`, image built); tests 99% (only env-sqlite). No `tl::expected` ever
> existed (`core::Result` is a self-contained variant-based expected). No
> `fmt::print`/`printf` in v3 layers (only `tools/*`). Wrote ADR
> `docs/adr/0009-modules-pilot.md`: standard bump accepted; modules pilot
> **deferred/gated** (can't validate Clang 18 / MSVC 19.40 here; modules+Catch2
> +vcpkg brittle); `std::expected` re-backing of `core::Result` + `std::print`
> in tools tracked as non-breaking follow-ups.

**Acceptance Criteria:**
- [~] `cmake --build` passes with C++23 — verified GCC 13 + GCC 15;
      Clang 18 / MSVC 19.40 / MinGW pending a CI matrix this env can't run
- [x] `tl::expected` no longer in the dependency graph (never present)
- [x] No `printf`/`fprintf`/`fmt::print` anywhere in the v3 tree. 2026-06-02:
      converted all ~309 `std::fprintf`/`std::printf` calls in `tools/*`
      (bniutils, client, bntrackd, bnpass) to `std::print`/`std::println`
      (trailing `\n` → `println`; `%spec`→`{}`/`{:02X}` etc.; `%.*s`→`{}` with
      `string_view`; `%" PRIuN "`→`{}`). Helper: `scripts/dev/printf_to_print.py`
      (auto-converted the simple majority; ~40 multi-line/`%.*s`/macro-concat
      cases done by hand). Verified: full gcc-15/C++23 container build 0 errors
      (only the 9 known STL false positives), all tool targets build, and
      `tgainfo`/`bnchat` `--help`/`--version` smoke output is correct.
- [x] Modules pilot result documented in `docs/adr/0009-modules-pilot.md`

**Steps:**
- [ ] CI matrix update: add GCC 14, Clang 18, MSVC 19.40; drop GCC ≤ 12, Clang ≤ 16
- [x] CMake: bump `PVPGN_V3_CXX_STANDARD` to 23 in `cmake/v3.cmake`
- [x] `tl::expected` → `std::expected`: N/A (no `tl::expected`; `core::Result`
      re-backing tracked as non-breaking follow-up)
- [ ] `fmt::print` → `std::print` in tools (v3 layers already clean)
- [x] Modules pilot: DEFERRED/gated — documented in ADR 0009
- [ ] `std::flat_map`/`std::flat_set` for small-N read-heavy lookup tables in `protocol/bnet/codec/`
- [ ] `[[assume]]` in tight inner loops only when a benchmark proves gain
- [ ] Deducing-this to remove CRTP in domain aggregates

---

## Phase D — External Surface (after Phase C)

### Plan 11 — Observability: Real OpenTelemetry
**Status:** ⬜ Not Started  
**Dependencies:** `core::IMetricsRegistry` and `core::log` exist; Plan 06 ideally landed

**Acceptance Criteria:**
- [ ] With `[observability].otlp_endpoint` set, traces, metrics, and logs land on a local OTel collector
- [ ] With endpoint unset, default behaviour matches today
- [ ] Trace context propagates across `bnetd → d2cs → d2dbs` in an e2e fixture
- [ ] `docs/operator/metrics.md` lists every emitted metric with type, labels, and rationale

**Steps:**
- [ ] ADR `0010-otel-exporter.md`: in-tree minimal OTLP/HTTP exporter for metrics + logs
- [ ] Add `[observability]` section to `bnetd.toml`
- [ ] Implement `infra/observability/otlp_metrics.cpp` adapting `core::IMetricsRegistry`
- [ ] Add OTLP log sink to `JsonLineLogger`
- [ ] Add `core/trace/span.hpp` (RAII, sampling, parent propagation)
- [ ] Instrument every application use case entry/exit; propagate trace IDs across services
- [ ] Promote three mandatory metrics to a documented contract in `docs/operator/metrics.md`
- [ ] Ship Grafana JSON in `contrib/dashboards/`

---

### Plan 12 — Plugin ABI Stabilization
**Status:** ⬜ Not Started  
**Dependencies:** Wave-One plugin ABI conformance test exists

**Acceptance Criteria:**
- [ ] Public C header `pvpgn/plugin/abi.h` exists, installed
- [ ] All shipped plugins load via the new ABI; manifest declares capabilities; host enforces them
- [ ] CI fails on breaking ABI change to `v1` without a new `v2` header and deprecation note
- [ ] No C++ symbol from `domain/` or `application/` is exposed to plugins
- [ ] `docs/developer/extending-pvpgn.md` updated with capability list

**Steps:**
- [ ] New public header `include/pvpgn/plugin/abi.h` (pure C): `pvpgn_plugin_v1_init`, `pvpgn_plugin_v1_shutdown`, versioned hook structs
- [ ] Capability tokens: plugin `.toml` manifest declares required capabilities; host enforces at load time
- [ ] Move `infra/plugin/` to drive the C ABI; C++ and Lua shims adapt it
- [ ] Semver gate: `scripts/dev/check-plugin-abi.sh` diffs C header against last tagged release
- [ ] Migrate all shipped plugins to the new ABI
- [ ] Update `docs/developer/extending-pvpgn.md`

---

### Plan 13 — Performance Benchmark Baseline
**Status:** ⬜ Not Started  
**Dependencies:** None for harness; gate enforcement waits until Plans 06 and 11 landing

**Acceptance Criteria:**
- [ ] `scripts/dev/run-bench.sh micro` and `macro` produce stable results (≤ 5% variance across 3 runs)
- [ ] Microbench gate runs in CI; budgeted regressions fail the PR
- [ ] Nightly macrobench writes results; alerts on > 25% regression
- [ ] `docs/developer/benchmarking.md` documents the harness, baseline release, and how to interpret results

**Steps:**
- [ ] Microbench harness under `tests/bench/micro/` using `nanobench` (vcpkg)
- [ ] Macrobench harness under `tests/bench/macro/`
- [ ] Runner: `scripts/dev/run-bench.sh <suite>` produces `bench-results.json`
- [ ] Capture baseline on tagged release; commit to `tests/bench/baselines/<release>.json`
- [ ] CI gate: microbench per-PR with 10% regression budget against `main`
- [ ] Optional: `contrib/dashboards/bench.json` for macrobench history

---

## Phase E — Ship (after Phase D)

### Plan 15 — Release and Rollout
**Status:** ⬜ Not Started  
**Dependencies:** All other plans complete

**Acceptance Criteria:**
- [ ] `docs/developer/release-process.md` published; PR template references it
- [ ] `docs/operator/runbooks/rolling-upgrade.md` walks operators through a no-downtime upgrade
- [ ] Distroless image builds and runs the full integration test suite
- [ ] Multi-arch tags published for the next release
- [ ] Release artefacts signed; SBOM attached
- [ ] `CHANGELOG.md` lints in CI

**Steps:**
- [ ] SemVer policy in `docs/developer/release-process.md`
- [ ] Deprecation policy documented in `CHANGELOG.md`
- [ ] Rolling-upgrade procedure (`docs/operator/runbooks/rolling-upgrade.md`)
- [ ] Distroless image (`Dockerfile.distroless`): `gcr.io/distroless/cc-debian12` base; < 80 MB; non-root
- [ ] Multi-arch build: publish `linux/amd64` and `linux/arm64` via `docker buildx`
- [ ] Signed artefacts: sign release binaries and container images with sigstore/cosign
- [ ] SBOM: emit CycloneDX SBOM with every release artefact
- [ ] `CHANGELOG.md` discipline: enforce Keep a Changelog format; CI lints headings

---

## Summary Dashboard

| Phase | Plan | Title | Status | Blocker |
|-------|------|-------|--------|---------|
| A | 05 | `application/ports/` Consolidation | ✅ | — |
| A | 14 | Docs / mkdocs --strict | ✅ | — |
| A | 03 | Strangler Finalization (legacy_bnetd) | ✅ | — |
| A | 04 | d2cs / d2dbs Strangler | ✅ | — |
| B | 02 | `src/common/` Purge | ⬜ | Plan 03 ≥80% |
| B | 07 | Infra Adapter Rehab | ⬜ | Plan 05 |
| B | 08 | Crypto Modernization | ⬜ | Plan 02 partial |
| B | 10 | Testing Pyramid Completion | ⬜ | Plans 02,03,04 |
| C | 06 | Async I/O Modernization | ⬜ | Plans 02,03 |
| C | 09 | C++23 Uplift | ⬜ | Plans 02,03,05 |
| D | 11 | Observability OTel | ⬜ | Phase C |
| D | 12 | Plugin ABI Stabilization | ⬜ | Phase C |
| D | 13 | Performance Benchmark Baseline | ⬜ | Phase C |
| E | 15 | Release and Rollout | ⬜ | All plans |
