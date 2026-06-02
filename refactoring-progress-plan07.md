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

## Remaining

- [ ] **game** — blocked on a domain change: `Game` has no `rehydrate` (only the
      validating `host()` factory), so a stored game can't be cleanly
      reconstructed. Needs a `Game::rehydrate` added to the aggregate first.
- [ ] **ladder** — port quirk: `get_rank` is by *name* but `LadderEntry` carries
      only an account *id*; resolve the id↔name path before consolidating.
      (account, channel, **account_ban**, **realm**, **friend_list**, **clan**,
      **ip_ban** done — **7/8**; only game + ladder remain, both blocked on
      domain/port decisions.)
- [ ] 🔒 Real driver matrix: implement/verify against sqlite (in-memory) +
      mysql/postgres via testcontainers in CI (env-gated locally).
- [ ] Delete the per-backend `infra/{sqlite,mysql,postgres}/*_repository.cpp`
      once each aggregate is consolidated; keep only the driver shims.
- [ ] Migrations: ensure an `account_bans` migration exists under
      `infra/migrations/account_ban/` with `-- dialect:` blocks.

## Acceptance criteria status

- [~] Exactly one `*_repository.cpp` per aggregate — done for account, channel,
      **account_ban**; remaining aggregates still have per-backend stubs.
- [ ] `infra/{sqlite,mysql,postgres}/` contain only driver adapters — pending
      deletion of consolidated aggregates' per-backend copies.
- [ ] CI runs the repository test matrix against all three backends — env-gated.
- [~] Switching `[storage].backend` needs no recompilation — true for the
      consolidated aggregates (driver injected at composition time).

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
