# Plan 07 — Infra Adapter Rehab — Progress

> Started: 2026-06-02 (this session)
> Plan: `plans/07-infra-adapter-rehab.md`
> Status legend: ✅ Done | 🔄 In Progress | ⬜ Not Started | 🔒 Env-gated

## Context (pre-session)

- The consolidated, driver-parameterized slice already existed for **account**
  and **channel** (`infra/persistence/{account,channel}_repository.cpp` over
  `IDbDriver`, wired in `RepositoryFactory`). All other aggregates
  (clan / ladder / realm / account_ban / ip_ban / friend_list / game) were
  `throw "Not yet implemented"` stubs in the factory, backed by `Unimplemented`
  per-backend repos.
- **Environment constraint:** sqlite is env-blocked here (`sqlite3.h` absent),
  so the existing `sql_account_repository_test` (in-memory sqlite) does not
  build/run locally. To test consolidated repos in *any* environment this
  session introduces a **recording fake `IDbDriver`**.

## Done (2026-06-02) — account_ban aggregate consolidated

- [x] **`infra/persistence/account_ban_repository.{hpp,cpp}`**
      (`SqlAccountBanRepository`) implements `domain::moderation::IAccountBanRepository`
      over the backend-agnostic `IDbDriver` — identical SQL/logic for
      sqlite/mysql/postgres, backend chosen at composition time (no recompile).
      - `find_active_ban` (param-bound SELECT; treats a stored-but-expired ban
        as *not active* via `AccountBan::active_at`),
      - `add_ban` (param-bound `INSERT OR REPLACE`; NULL `expires_at` =
        permanent),
      - `remove_ban` (param-bound DELETE),
      - `for_each` (streamed mapping, predicate-controlled).
      - SystemTime ↔ unix epoch seconds; `AccountId` ↔ INTEGER. All writes are
        **parameter-bound** (injection-safe), unlike the older account repo's
        string interpolation.
- [x] **Factory**: `RepositoryFactory::create_account_ban_repository()` now
      returns the driver-injected `SqlAccountBanRepository` (was a stub throw).
- [x] **CMake**: added `account_ban_repository.cpp` to the `infra_persistence`
      object library. Builds locally (no sqlite needed for repos over IDbDriver).
- [x] **Test** (`tests/unit/infra/persistence/sql_account_ban_repository_test.cpp`,
      7 cases / 39 assertions, **green, no sqlite**): a **recording fake
      `IDbDriver`** (records SQL + bound params, replays programmable rows) +
      a programmable `FakeRow`. Verifies the bound upsert + params, NULL-expiry
      binding, row→domain mapping, active-vs-expired/permanent logic, the bound
      DELETE, and predicate-controlled `for_each`. The same repo runs unchanged
      over the real drivers; this pins its logic in isolation.

### Reusable testing pattern established
The recording fake `IDbDriver` makes every consolidated repository unit-testable
without a live DB — the right complement to the sqlite/testcontainers
integration matrix (which stays env-gated here).

## Done (2026-06-02) — realm aggregate consolidated + shared fake-driver header

- [x] **Promoted the recording fake driver to a shared header**
      `tests/unit/infra/persistence/recording_fake_driver.hpp`
      (`pvpgn::test::persistence::{RecordingFakeDriver, FakeRow, Cell, as_int,
      as_str, is_null_param}`); refactored the account_ban test to use it
      (still 7 cases / 39 assertions green).
- [x] **`infra/persistence/realm_repository.{hpp,cpp}`** (`SqlRealmRepository`)
      implements `domain::realm::IRealmRepository` over `IDbDriver` — mirrors
      the account slice's shape: `find_by_id`, `find_by_name`
      (`COLLATE NOCASE`), `save` (param-bound upsert), `remove` (param-bound
      DELETE), `forEach`, `size` (`COUNT(*)`). Row → `Realm` rehydration uses
      `Realm::create` + `unregister()` for the stored `active` flag, then
      `drain_events()` to discard reconstruction events.
- [x] **Factory**: `create_realm_repository()` returns the driver-injected
      `SqlRealmRepository` (was a stub throw).
- [x] **CMake**: `realm_repository.cpp` added to `infra_persistence`.
- [x] **Test** (`sql_realm_repository_test.cpp`, 8 cases / 36 assertions,
      **green, no sqlite**): bound upsert/params, id/name lookup + binding,
      active vs inactive rehydration, NotFound, bound DELETE, `COUNT(*)` size,
      predicate-controlled `forEach`.

## Done (2026-06-02) — friend_list aggregate consolidated (transactional)

- [x] **`infra/persistence/friend_list_repository.{hpp,cpp}`**
      (`SqlFriendListRepository`) implements `domain::social::IFriendListRepository`
      over `IDbDriver`. One-to-many `friends(owner_id, friend_id, position)`
      table; order preserved via `position`.
      - `find_by_owner`: ordered SELECT → `FriendList::rehydrate` (an owner with
        no rows yields a valid **empty** list, not NotFound).
      - `save`: **transactional full-replace** — `begin` → `DELETE` →
        ordered `INSERT`s → `commit`, with `rollback` on any failure.
- [x] **Factory** `create_friend_list_repository()` wired (was a stub throw).
- [x] **CMake**: `friend_list_repository.cpp` added to `infra_persistence`.
- [x] Extended the shared fake driver with `begin/commit/rollback` counters
      (additive — existing account_ban/realm tests stay green).
- [x] **Test** (`sql_friend_list_repository_test.cpp`, 4 cases / 33 assertions,
      **green, no sqlite**): ordered read, empty-list-not-NotFound, the
      transactional DELETE + ordered INSERTs (begin=1/commit=1/rollback=0 with
      correct positions), and empty-save = DELETE-only.

## Done (2026-06-02) — clan aggregate consolidated (two-table, transactional)

- [x] **`infra/persistence/clan_repository.{hpp,cpp}`** (`SqlClanRepository`)
      implements `domain::social::IClanRepository` over `IDbDriver`. Clan is a
      parent row + ordered member list:
      `clans(id, tag, name, client_tag)` + `clan_members(clan_id, account_id,
      rank, position)`.
      - `find_by_id`/`find_by_tag`/`find_by_name` (`COLLATE NOCASE` for name)
        each issue **two queries** — the `clans` header then `clan_members`
        ordered by position — and `Clan::rehydrate` them into a
        `shared_ptr<Clan>`. `ClientTag` round-trips via its 4-char `text()`;
        an unparseable stored tag is rejected (`Internal`), absent header is
        `NotFound`.
      - `save`: transactional — `begin` → upsert `clans` → delete + ordered
        re-insert of `clan_members` → `commit` (rollback on any failure).
      - `remove(tag)`: transactional — delete members
        (`clan_id IN (SELECT id FROM clans WHERE tag = ?)`) then the clan row.
- [x] **Factory** `create_clan_repository()` wired (was a stub throw).
- [x] **CMake**: `clan_repository.cpp` added to `infra_persistence`.
- [x] Extended the shared fake driver with a **per-query result-set queue**
      (`push_result_set` / `result_sets`) so multi-query repos can program
      distinct results per query — backward-compatible with `next_rows`
      (existing tests stay green).
- [x] **Test** (`sql_clan_repository_test.cpp`, 6 cases / 50 assertions,
      **green, no sqlite**): two-query load + ordered members, NotFound,
      tag/name binding, invalid-client_tag rejection, the 4-statement
      transactional save (with positions + client-tag text), and the
      two-statement tag-scoped remove.

## Done (2026-06-02) — ip_ban aggregate consolidated (two-table, CIDR)

- [x] **`infra/persistence/ip_ban_repository.{hpp,cpp}`** (`SqlIpBanRepository`)
      implements the 8-method `domain::moderation::IIpBanRepository` over
      `IDbDriver`. Two tables: `ip_bans` (exact host) + `ip_ban_ranges` (CIDR).
      - `is_banned` mirrors the in-memory adapter (samples the clock itself,
        excludes expired): exact hit filtered in SQL, then **in-process CIDR
        match** (`cidr_match`, replicating the aggregate's private prefix logic
        over `IpAddress` v4/v6 bytes) against active ranges.
      - `add_ban`/`add_range_ban`/`remove_ban`/`remove_range_ban`: bound
        writes; `for_each_entry`: streamed mapping.
      - `load_banlist`/`save_banlist`: round-trip the **exact-host entries only**
        (the `IpBanList` aggregate does not expose its range list, and
        `rehydrate` takes entries only — documented limitation; ranges are
        managed via the granular range methods). `save_banlist` is transactional
        (DELETE all + re-insert).
      - IPs persist as `to_string()` text (v4/v6); SystemTime ↔ epoch seconds.
- [x] **Factory** `create_ip_ban_repository()` wired (was a stub throw).
- [x] **CMake**: `ip_ban_repository.cpp` added to `infra_persistence`.
- [x] **Test** (`sql_ip_ban_repository_test.cpp`, 9 cases / 47 assertions,
      **green, no sqlite**): bound exact/range writes + NULL expiry, is_banned
      via exact hit and via in-process CIDR match (in-range true, out-of-range
      false), bound removes, for_each mapping, load_banlist rehydration, and the
      transactional save_banlist.

## Done (2026-06-02) — game + ladder consolidated (ALL aggregates done)

- [x] **game** — added a `Game::rehydrate(id, host, client, desc, state,
      players)` factory to the domain aggregate (reconstructs persisted state
      without events or `host()` validation; for repositories only). Then
      `infra/persistence/game_repository.{hpp,cpp}` (`SqlGameRepository`): two
      tables `games` + ordered `game_players`; `find_by_id`/`find_by_name`
      (header query → players query → `rehydrate` into `shared_ptr<Game>`),
      transactional `save`, name-scoped `remove`, and `list_active`
      (`state <> Finalized`, players loaded per game). Factory wired.
      Test `sql_game_repository_test` (6 cases / 50 assertions green).
- [x] **ladder** — fixed the port quirk **and a latent bug**:
      `ILadderRepository::get_rank` was by *name* (`std::string_view`), but
      `LadderEntry`/`save_entry` carry only an account *id*, and the real caller
      (`get_ladder_entry.cpp`) passed the username while the in-memory impl
      compared it against `std::to_string(id)` — so get_rank could never match.
      Changed the port to `get_rank(domain::AccountId)`; updated the in-memory
      impl (compare by id), the sqlite stub, the caller (pass `query.account_id`),
      and the 4 affected ladder/inmemory test mocks. Then
      `infra/persistence/ladder_repository.{hpp,cpp}` (`SqlLadderRepository`):
      `ladder` table; `get_rank` = lookup rating then `1 + COUNT(rating > r)`
      (ties share a rank; NotFound when off the ladder), bound `save_entry`
      upsert, ordered+limited `get_top_n`. Factory wired. Test
      `sql_ladder_repository_test` (5 cases / 28 assertions green); all 33
      ladder-related ctest tests still pass.

**ALL repository aggregates are now consolidated over `IDbDriver`** (account,
channel, account_ban, realm, friend_list, clan, ip_ban, game, ladder). 7 new
fake-driver repo tests total 283 assertions / 45 cases, green with no sqlite.

## Done (2026-06-02) — SQLiteUnitOfWork migrated onto the consolidated repos 🔒

> ⚠ **Env-gated / UNVERIFIED:** sqlite does not build here (no `sqlite3.h`), so
> the files below could not be compiled. The local non-sqlite build still
> configures and the consolidated repo tests still pass (the change only touches
> env-gated TUs). **A sqlite-capable build must validate this before merge.**

Finding: the per-backend `infra/sqlite/*_repository.*` were NOT dead — they were
the live persistence path (`SQLiteUnitOfWork` ← `bnetd/main.cpp:290` via
`SQLiteUnitOfWorkFactory`). The consolidated `RepositoryFactory` existed in
parallel but the app/UoW had never been switched to it.

- [x] **`SQLiteUnitOfWork` now constructs the consolidated repos**
      (`persistence::Sql{Account,Clan,Ladder,IpBan,AccountBan,FriendList,Realm,
      Channel}Repository`) over a `persistence::SqliteDriver` built from its
      connection; members held by their domain interfaces. `games_`/`teams_`
      stay in-memory (session-scoped, unchanged).
- [x] **Transaction nesting:** `SqliteDriver` is now SAVEPOINT-aware — the
      outermost begin/commit/rollback maps to `BEGIN/COMMIT/ROLLBACK`, inner
      ones to `SAVEPOINT/RELEASE/ROLLBACK TO` (depth counter). `SQLiteUnitOfWork::
      begin/commit/rollback` route through the driver, so a repo's own
      multi-statement transaction nested inside a UoW transaction no longer hits
      SQLite's "cannot start a transaction within a transaction".

## Remaining
- [ ] 🔒 **Validate the UoW migration on a sqlite build** (the change above) —
      especially the SAVEPOINT nesting under real BEGIN/COMMIT.
- [ ] Delete the now-unused per-backend repos — a larger env-gated cascade:
      `infra/sqlite/{account,account_ban,clan,friend_list,ip_ban,ladder,realm}_repository.*`
      + `sqlite_channel_repository.*`; the deprecated shim
      `infra/persistence/sqlite/`; the `infra/{mysql,postgres}/account_repository.*`
      (dead); and the per-backend account tests
      (`tests/unit/infra/sqlite/sqlite_account_repository_test.cpp`,
      `tests/integration/account_repository_integration_test.cpp`) + all the
      matching CMake entries. Best done on a sqlite-capable build so the link
      can be verified.
- [ ] 🔒 Real driver matrix: implement/verify against sqlite (in-memory) +
      mysql/postgres via testcontainers in CI (env-gated locally).
- [ ] Migrations: ensure an `account_bans` migration exists under
      `infra/migrations/account_ban/` with `-- dialect:` blocks.

## Acceptance criteria status

- [x] Exactly one consolidated `*_repository.cpp` per aggregate — **all 9
      aggregates done** (account, channel, account_ban, realm, friend_list,
      clan, ip_ban, game, ladder) over `IDbDriver`.
- [ ] `infra/{sqlite,mysql,postgres}/` contain only driver adapters — pending
      deletion of the now-superseded per-backend `*_repository.cpp` copies.
- [ ] CI runs the repository test matrix against all three backends — env-gated
      (sqlite has no headers here; mysql/postgres need testcontainers).
- [x] Switching `[storage].backend` needs no recompilation — the consolidated
      repos take the driver at composition time; switching backend = a different
      driver, no repo/factory recompile.

## Log
- 2026-06-02: consolidated the `account_ban` aggregate onto `IDbDriver`, wired
  the factory, and established the recording-fake-driver test pattern
  (7 cases / 39 assertions green, no sqlite).
- 2026-06-02: promoted the fake driver to a shared header; consolidated the
  `realm` aggregate the same way (8 cases / 36 assertions green). 4 of ~8
  aggregates now consolidated (account, channel, account_ban, realm).
- 2026-06-02: consolidated `friend_list` (transactional full-replace save);
  added transaction counters to the shared fake driver
  (4 cases / 33 assertions green). 5 of ~8 aggregates consolidated.
- 2026-06-02: consolidated `clan` (two-table parent+members, transactional
  save/remove); added a per-query result-set queue to the shared fake driver
  (6 cases / 50 assertions green). 6 of ~8 aggregates consolidated.
- 2026-06-02: consolidated `ip_ban` (two-table exact + CIDR ranges; in-process
  CIDR matching for is_banned; transactional save_banlist) — 9 cases /
  47 assertions green. 7 of ~8 aggregates consolidated; only game (needs
  Game::rehydrate) and ladder (port id/name quirk) remain.
- 2026-06-02: consolidated `game` (added `Game::rehydrate`; two-table
  games+players; list_active) — 6 cases / 50 assertions. Consolidated `ladder`
  (fixed `get_rank` port id/name quirk + latent bug; rank via COUNT) — 5 cases /
  28 assertions. **ALL 9 aggregates now consolidated over IDbDriver.**
