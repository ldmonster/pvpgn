# Storage round-trip bug hunt: account/data persistence (v3 vs original)

Subsystem: account/data storage round-trip — does every field WRITTEN by a
repository get READ back correctly, and does the SQL the repos emit actually
match the migration schema that runs in production?

Scope examined (v3, `/home/cnupt/work/pvpgn`):
- `src/infra/persistence/{account,channel,clan,friend_list,ip_ban,ladder,realm,account_ban}_repository.cpp`
- `src/infra/migrations/{src/all_migrations.cpp, sql/001_initial_schema.sql, sql/002_channels.sql, account/V0001__create_accounts.sql, src/migration_runner.cpp}`
- `src/infra/sqlite/src/connection.cpp`
- `src/infra/file/src/account_repository.cpp` + `flat_db_reader`
- `src/infra/persistence/repository_factory.cpp` (which repos are wired to prod)

Compared conceptually against original `src/bnetd/storage_file.cpp` / `storage_sql.cpp`.

## TL;DR — count of write/read mismatches

**8 distinct write/read mismatches found.** 5 are HIGH (production data
loss/total breakage); 2 MEDIUM; 1 LOW. The dominant problem is a **systemic
schema/repository divergence**: the SQL the production `Sql*Repository` classes
emit references **tables and columns that do not exist in the only schema the
runtime creates** (`all_migrations.cpp` / `001_initial_schema.sql`). The repo
unit tests use a *fake recording driver* that only string-matches the SQL, so
they never run against the real schema and the divergence is invisible to CI.

`RepositoryFactory` (repository_factory.cpp:42-128) wires the `Sql*Repository`
classes for the `sqlite`/`mysql`/`postgres` backends — i.e. these are the
production code paths, not test doubles.

---

## Per-repository SAVE-fields vs FIND-fields vs SCHEMA

Legend: ✅ matches schema · ❌ table/column absent from schema · ⚠️ written-but-never-read or read-but-never-written.

### accounts — round-trips internally, schema OK, two soft issues
| field | SAVE (account_repository.cpp:178-197) | FIND (`account_from_row`, :51-90) | schema (all_migrations.cpp:18-28) |
|---|---|---|---|
| id | ✅ idx0 | ✅ get_int(0) | id |
| name | ✅ | ✅ get_text(1) | name |
| locale | ✅ | ✅ get_text(2) | locale |
| password_hash | ✅ hex TEXT | ✅ hex TEXT | **BLOB** (see F-ACC-1) |
| locked | ✅ | ✅ get_int(4) | locked |
| must_change_password | ✅ | ✅ get_int(5) | must_change_password |
| command_groups | ✅ CSV | ✅ CSV parse | command_groups |
| created_at | writes literal `0` | not read | created_at |
| updated_at | writes literal `0` | not read | updated_at |

Column order in SELECT (account_repository.cpp:101) matches the indices in
`account_from_row` exactly — no off-by-one. Round-trips cleanly for the live
fields. Two sub-issues below.

### channels — clean ✅
| field | SAVE (:95-101) | FIND (`channel_from_row`, :12-33) | schema (all_migrations.cpp:114-121) |
|---|---|---|---|
| id | ✅ | get_int(0) | id |
| name | ✅ | get_text(1) | name |
| topic | ✅ | get_text(2) | topic |
| flags | ✅ bitmask int | get_int(3) bit-decode | flags |
| max_members | ✅ | get_int(4) | max_members |

SELECT order (:47) == row indices. `created_at` in schema is never written by
save (DB default fills it) and never read — harmless. **Clean round-trip.**

### account_bans — clean ✅
SAVE (:82-85) `account_id, banned_by, reason, banned_at, expires_at`.
FIND (`ban_from_row`, :27-41) reads the same 5 columns, same order
(kSelectCols :22). Schema (all_migrations.cpp:38-44) has exactly those columns.
epoch<->SystemTime symmetric; NULL `expires_at` guarded with `is_null(4)`.
**Clean round-trip.**

### realms — clean for the columns it uses ✅ (one dropped column)
SAVE (:92-93) and FIND (kCols :7 = `id, name, description, active`) agree, same
order. Schema (all_migrations.cpp:95-101) has `id,name,description,host,port,active`.
`host` and `port` exist in schema but are **never written and never read** by
the repo — see F-REALM-1 (MEDIUM). The columns the repo *does* touch round-trip
cleanly and the SELECT order matches the row indices.

### clans / clan_members — BROKEN ❌ (F-CLAN-1, HIGH)
| repo SQL | schema `clans` (all_migrations.cpp:57-63) | schema `clan_members` (:67-72) |
|---|---|---|
| SELECT/INSERT `clans(id, tag, name, client_tag)` | id, tag, name, **founder_id, motd, created_at** | — |
| `clan_members(clan_id, account_id, rank, position)` `ORDER BY position` | — | clan_id, account_id, rank, **joined_at** |

- `clans.client_tag` — **does not exist** in schema (`founder_id` does, but repo
  never writes/reads it). INSERT and SELECT both fail at runtime.
- `clan_members.position` — **does not exist** in schema (column is `joined_at`).
  Both the INSERT (clan_repository.cpp:129) and the `ORDER BY position`
  (clan_repository.cpp:22) fail.
- `founder_id`, `motd`, `clans.created_at`, `clan_members.joined_at` — schema
  columns the repo never populates.

### friend_lists — BROKEN ❌ (F-FRIEND-1, HIGH)
| repo SQL (friend_list_repository.cpp) | schema (all_migrations.cpp:75-80) |
|---|---|
| table **`friends`**, cols `owner_id, friend_id, position` | table **`friend_lists`**, cols `owner_id, friend_id, added_at` |

Repo reads/writes a table named `friends` (`:22, :52, :62`) — schema only
defines `friend_lists`. Repo uses a `position` column for ordering; schema has
no `position` (it has `added_at`). Every save/find fails at runtime.

### ladder — BROKEN ❌ (F-LADDER-1, HIGH)
| repo SQL (ladder_repository.cpp) | schema `ladder_entries` (all_migrations.cpp:82-91) |
|---|---|
| table **`ladder`**, cols `account_id, rating, wins, losses, disconnects` | table **`ladder_entries`**, cols `account_id, client_tag, wins, losses, disconnects, rating, rank, updated_at` |

- Table name mismatch: repo uses `ladder`, schema defines `ladder_entries`.
- `client_tag` is part of the schema PRIMARY KEY `(account_id, client_tag)` and
  is `NOT NULL` — the repo never writes it, so even a corrected table name would
  fail the NOT NULL/PK constraint. The repo treats the ladder as keyed by
  `account_id` alone, losing the per-client-tag dimension the original server
  (and the schema) model.
- Column index order in `entry_from_row` (`rating`=idx1, `wins`=idx2…) matches
  the repo's own SELECT (kCols :11-12), so it is internally consistent — but
  against `ladder_entries` the order would be wrong too (there `client_tag` is
  col1, `rating` is col5).

### ip_bans — BROKEN ❌ (F-IPBAN-1, HIGH)
| repo SQL (ip_ban_repository.cpp) | schema `ip_bans` (all_migrations.cpp:46-52) |
|---|---|
| `ip_bans(ip, reason, issuer, issued_at, expires_at)` | `ip_bans(id, ip_address, is_range, banner_account_id, reason, banned_at, expires_at)` |
| also table **`ip_ban_ranges(network, prefix_bits, reason, issuer, issued_at, expires_at)`** | **no such table** |

- `ip` vs `ip_address`, `issuer` vs `banner_account_id`, `issued_at` vs
  `banned_at` — column names disagree on every field. SELECT/INSERT fail.
- `ip_ban_ranges` table referenced by `add_range_ban`/`remove_range_ban`/
  `is_banned` CIDR path does not exist in the schema at all.
- (A separate finding file `bug-hunt/findings/moderation-ipban.md` already
  exists; this confirms the storage-layer half: the two-table model the repo
  assumes was never migrated.)

---

## Findings

### F-CLAN-1 — Clan repo SQL targets non-existent columns — HIGH / BUG
- v3 save: `clan_repository.cpp:110-111` (`INSERT ... clans (id, tag, name, client_tag)`),
  `:129` (`clan_members (clan_id, account_id, rank, position)`).
- v3 find: `clan_repository.cpp:47` (`SELECT id, tag, name, client_tag FROM clans`),
  `:21-22` (`SELECT account_id, rank FROM clan_members ... ORDER BY position`).
- Schema: `all_migrations.cpp:57-72`.
- Mismatch: `clans.client_tag` and `clan_members.position` do not exist;
  schema's `founder_id/motd/created_at/joined_at` are never used. Production
  INSERT/SELECT error out → clans cannot be saved or loaded.
- Fix: reconcile schema and repo. Either add `client_tag`/`position` columns to
  the migration (and drop or default the unused `founder_id/motd`), or rewrite
  the repo to the schema's columns. Whichever wins, drive at least one repo test
  against the real migration SQL (see "Root cause" below).

### F-FRIEND-1 — Friend repo uses table `friends`/col `position`; schema has `friend_lists`/`added_at` — HIGH / BUG
- v3 save: `friend_list_repository.cpp:52, 62`. find: `:22`.
- Schema: `all_migrations.cpp:75-80`.
- Fix: rename to `friend_lists` (or migrate a `friends` table) and add a
  `position` column, or order by `added_at`.

### F-LADDER-1 — Ladder repo uses table `ladder`, omits NOT NULL `client_tag` — HIGH / BUG
- v3 save: `ladder_repository.cpp:67-68`. find: `:36, :87` (kCols :11-12).
- Schema: `all_migrations.cpp:82-91` (table `ladder_entries`, PK includes `client_tag`).
- Fix: target `ladder_entries`, thread `client_tag` through `LadderEntry` and the
  PK; column read order must follow `ladder_entries` layout.

### F-IPBAN-1 — IP-ban repo column names + `ip_ban_ranges` table absent from schema — HIGH / BUG
- v3: `ip_ban_repository.cpp:121-122, 138-139, 84-85, 97-98, 172, 187`.
- Schema: `all_migrations.cpp:46-55` (no `ip_ban_ranges`; different column names).
- Fix: add the `ip_ban_ranges` migration and align `ip`/`ip_address`,
  `issuer`/`banner_account_id`, `issued_at`/`banned_at` between repo and schema.

### F-SCHEMA-DUP — Three divergent "account" schema definitions — HIGH / BUG
- Definitions: `all_migrations.cpp:18-28` (runtime, `password_hash BLOB`),
  `sql/001_initial_schema.sql:13-23` (BLOB), and
  `migrations/account/V0001__create_accounts.sql:6-16` (`password_hash TEXT`,
  `name ... UNIQUE` with no NOCASE).
- Only `all_migrations.cpp` is embedded/applied by the runtime (the `.sql` files
  are not read by `migration_runner` — it consumes the embedded `Migration`
  array). The `V0001` file uses TEXT for the hash and a plain-UNIQUE name index,
  diverging from the live schema. Risk: whichever path a backend tool loads, the
  account table shape differs.
- Fix: collapse to a single source of truth for the schema; delete or generate
  the stray `.sql` copies from the embedded migrations.

### F-ACC-1 — `password_hash` stored as 40-char hex TEXT into a `BLOB NOT NULL` column — MEDIUM / BUG
- v3 save: `account_repository.cpp:185` writes `bn_hash_to_hex(...)` (ASCII hex)
  as a quoted string literal.
- v3 find: `account_repository.cpp:66` reads via `get_text(3)` then
  `bn_hash_from_hex`.
- Schema: column is `password_hash BLOB NOT NULL` (all_migrations.cpp:22).
- Round-trip: SQLite's type affinity is permissive, so a hex *string* stored in
  a BLOB-affinity column is kept as TEXT and read back as TEXT — so on SQLite it
  *happens* to round-trip. But (a) on stricter backends (Postgres `BYTEA`,
  MySQL `BLOB`) the same INSERT of a quoted ASCII string vs the declared binary
  type is a type mismatch / silent re-encoding, and (b) the declared BLOB intent
  vs the hex-TEXT reality is a latent corruption trap if any tool ever reads the
  column as raw bytes. Note the conflicting `V0001` schema declares it TEXT.
- Fix: make the schema and the writer agree — either declare the column TEXT
  (40-hex) everywhere, or bind a real blob (`bind_blob` of the 20 raw bytes) and
  read with `get_blob`.

### F-ACC-2 — `created_at`/`updated_at` always written as literal `0` — LOW / BUG (silent metadata loss)
- v3 save: `account_repository.cpp:189` writes `0, 0`; the ON CONFLICT clause
  (:197) sets `updated_at = excluded.updated_at` (also 0).
- Never read back (`account_from_row` ignores cols 7/8).
- Effect: account creation/update timestamps are permanently 0. Not a
  correctness bug for the live identity fields, but the timestamps the original
  server tracks (acct\\lastlogin\\time etc.) are silently dropped. LOW because
  nothing reads them today.

### F-REALM-1 — Realm `host`/`port` columns never written or read — MEDIUM / BUG (data loss)
- Schema `realms` has `host TEXT NOT NULL` and `port INTEGER NOT NULL`
  (all_migrations.cpp:98-101), but the repo's `kCols` is `id, name, description,
  active` (realm_repository.cpp:7) and `save` (:92) only inserts those four.
- `host NOT NULL`/`port NOT NULL` have no DEFAULT → on the live schema the
  INSERT **fails the NOT NULL constraint** (so realm save is broken too), and
  even if it didn't, the realm's network address would be unreadable. The domain
  `Realm` aggregate (realm_from_row :13-16) carries no host/port, so the address
  a D2 client needs to connect is lost end-to-end.
- Fix: either drop host/port from the schema (give them defaults) if the realm
  address lives elsewhere, or thread host/port through the `Realm` aggregate and
  the repo.

---

## File-backed account repository (`.plain`) — round-trip check

`FileAccountRepository::save` (file/src/account_repository.cpp:199-214) writes:
`username, passhash1, auth_lock, auth_command_groups, locale, userid`.
`load_account_file` (:128-180) reads exactly those keys back. Internally
symmetric and field names match. No SQL-schema mismatch (it's flat key=value).

Caveats vs the original `storage_file.cpp` (classification SAFE-VERIFIED for the
round-trip, but **narrower than original** by design):
- `must_change_password` is written nowhere and hard-coded `false` on load
  (:179) — the v3 model simply doesn't persist it in the file backend.
- The original `.plain` format carries dozens of attributes (passhash2, email,
  profile\\*, sys\\*, lastlogin, BNLS tokens, etc.). v3's file backend stores
  only the 6 fields above. That's a deliberate scope reduction, not a
  write/read asymmetry — every field v3 *writes* it also *reads*. Flagged for
  awareness, not as a round-trip bug.

---

## Repos that round-trip cleanly (coverage)

- **channels** — SAVE/FIND/schema all agree, indices aligned. ✅
- **account_bans** — SAVE/FIND/schema agree, epoch + NULL handling symmetric. ✅
- **accounts** — live fields round-trip; SELECT order matches indices; only the
  BLOB-vs-hex (F-ACC-1) and timestamp=0 (F-ACC-2) soft issues. ✅ (modulo F-ACC-1/2)
- **file `.plain` account repo** — internally symmetric. ✅
- **clan rank enum<->byte** — `clan_rank_to_wire`/`clan_rank_from_wire`
  (clan_rank_wire.hpp:42-64) are symmetric; save uses to_wire (clan_repository.cpp:132),
  load uses from_wire (:27). The recently-fixed mapping round-trips correctly. ✅
  (The *clan table* is still broken per F-CLAN-1 — the rank value is fine, the
  columns around it are not.)

Broken (cannot round-trip in production): **clans, friend_lists, ladder, ip_bans**.

---

## Root cause / systemic note

The four HIGH breakages share one cause: **the repo unit tests use a fake
recording driver** (`tests/unit/infra/persistence/recording_fake_driver.hpp`)
that records the SQL string and replays canned `FakeRow`s. Tests assert the SQL
*text* (e.g. `find("INSERT OR REPLACE INTO ladder")`, `find("ORDER BY
position")`) and feed rows shaped to the repo's own assumptions. Nothing ever
executes the repo SQL against the schema produced by `all_migrations.cpp`. So
the repo and the migration drifted independently and CI stayed green.

Recommended structural fix (beyond the per-repo column reconciliations): add at
least one integration test per repo that (1) runs the real embedded migrations
into an in-memory SQLite DB, then (2) exercises save→find through the actual
`SQLiteConnection`. That single harness would have caught all four HIGH
findings and F-REALM-1's NOT NULL failure at once.

(Read-only audit; no source modified.)
