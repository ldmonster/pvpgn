# SQL Injection / Query-Construction Audit — v3 persistence layer

Scope: every SQL-building repository under `src/infra/persistence/` plus the
MySQL/Postgres account repos and the SQLite driver. Method: for each
save/find/remove/query, classify SQL construction (bound `?`/`$n` params vs raw
concatenation), identify which concatenated values are user-controllable, and
trace upstream validation to decide exploitability.

Driver API note: `IDbDriver::query_bind(sql, {params...}, cb)` (see
`src/infra/persistence/sql_builder/db_driver.hpp:52`) is the bound-parameter API
and **already exists and is used by almost every repo**. The two SQLite-path
writes below are the outliers that build SQL by raw `std::ostringstream <<`.

Stacked-query amplifier: `SQLiteConnection::exec` calls `sqlite3_exec`
(`src/infra/sqlite/src/connection.cpp:81`), which executes **all**
semicolon-separated statements in the string. Any injection through `exec()` is
therefore a full stacked-query injection (DROP/UPDATE/etc.), not just a clause
break.

---

## SUMMARY

- CRIT injection sites: **2** (channel name, channel topic — both
  remote/low-priv, reach raw `sqlite3_exec`)
- HIGH injection sites: **0**
- Fragile-but-not-exploitable (raw concat, but field charset-restricted): 3
  (SQLite/MySQL/Postgres account `save`)
- SAFE / fully parameterized repos: account (find/remove/forEach), clan, friend,
  ip_ban, ladder, realm, account_ban, game — all methods.

---

## FINDING 1 — CRIT: Channel name SQL injection (unescaped, stacked)

- Classification: **BUG (SQL injection)**
- Severity: **CRIT** — remotely reachable by any authenticated client via a
  single `SID_JOINCHANNEL` packet; joining a not-yet-existing channel auto-
  creates it, so no special privilege is required. Reaches `sqlite3_exec`
  ⇒ stacked queries (arbitrary DDL/DML, e.g. privilege escalation or table drop).
- v3 ref: `src/infra/persistence/channel_repository.cpp:94-102`
  ```cpp
  std::ostringstream sql;
  sql << "INSERT OR REPLACE INTO channels (id, name, topic, flags, max_members) "
      << "VALUES ("
      << static_cast<std::int64_t>(channel.id().value()) << ", '"
      << channel.name() << "', '"      // <-- name concatenated raw, no escape
      << channel.topic() << "', "      // <-- topic concatenated raw, no escape
      << static_cast<int>(flags_raw) << ", "
      << channel.policy().max_members << ");";
  return driver_->exec(sql.str());     // sqlite3_exec -> multi-statement
  ```
- Field + attacker control: `channel.name()` is a plain `std::string` on the
  aggregate (`domain/chat/channel.hpp:84`); `Channel::create`/`rehydrate` accept
  it with **no validation**. The value originates from the wire:
  `BnetFsm::on(JoinChannel)` passes `m.channel` straight through
  (`src/protocol/bnet/src/fsm/fsm_chat.cpp:89-90`) to
  `JoinChannel::execute(...)`. That handler
  (`src/application/chat/src/join_channel.cpp:18-40`) only rejects empty names
  and names containing `\0` or `\x01` — it does **not** call
  `domain::chat::ChannelName::parse` and does **not** reject `'`, `;`, or `--`.
  On a cache miss it calls `Channel::create(..., channel_name, ...)` then
  `channel_repo_.save(channel)`. (Same vector is also reachable via the IRC
  `JOIN` path.)
  - Note: `ChannelName::parse` (`domain/chat/channel_name.hpp:38`) would still
    not help — it permits all printable ASCII `0x20..0x7E`, which includes `'`
    `;` `-`. The aggregate never enforces even that, but charset validation is
    not a SQL-safety substitute regardless.
- Example malicious value (channel name):
  `x',0,0); DROP TABLE accounts;--`
  yields `...VALUES (0, 'x',0,0); DROP TABLE accounts;--', '', 0, 0);` — the
  injected `DROP TABLE accounts` runs as a second statement.
- Fix: use the existing bound-parameter API. Replace the `ostringstream`/`exec`
  with:
  ```cpp
  return driver_->query_bind(
      "INSERT OR REPLACE INTO channels (id, name, topic, flags, max_members) "
      "VALUES (?, ?, ?, ?, ?)",
      {static_cast<std::int64_t>(channel.id().value()),
       channel.name(), channel.topic(),
       static_cast<std::int64_t>(flags_raw),
       static_cast<std::int64_t>(channel.policy().max_members)},
      [](const DbRow&){ return false; });
  ```
  (`query_bind` already exists; the clan/game/realm repos do exactly this.)

## FINDING 2 — CRIT: Channel topic SQL injection (unescaped, stacked)

- Classification: **BUG (SQL injection)**
- Severity: **CRIT** — same sink as Finding 1.
- v3 ref: same statement, `src/infra/persistence/channel_repository.cpp:99`
  (`<< channel.topic() << "', "`).
- Field + attacker control: `channel.topic()` is set by
  `Channel::set_topic(moderator, std::string topic)`
  (`domain/chat/channel.hpp:162`) with **no charset restriction**. The
  application handler `SetChannelTopic::execute`
  (`src/application/chat/src/set_channel_topic.cpp:13-31`) only length-checks
  (`MAX_TOPIC_LENGTH = 255`) and otherwise forwards the raw client topic string.
  Setting a topic requires only channel membership (a low-priv, authenticated
  action), after which `channels_->save(channel)` concatenates the topic raw.
  Topic is exactly the kind of free-text field that legitimately contains
  apostrophes, so this is trivially triggered.
- Example malicious value (topic):
  `gg', 0, 0); UPDATE accounts SET command_groups='1,2,3,4,5,6,7,8' WHERE name='attacker';--`
  → privilege escalation via the second statement.
- Fix: the Finding-1 `query_bind` rewrite binds `channel.topic()` as a parameter
  and closes this simultaneously. Single fix covers both findings.

---

## FRAGILE-BUT-NOT-EXPLOITABLE (raw concat, charset-restricted field)

### A. SQLite account `save` — `src/infra/persistence/account_repository.cpp:178-199`
Builds the upsert with `std::ostringstream`, concatenating `account.name()
.display()`, `account.locale().text()`, the hex password hash, and the
`command_groups` string with **no escaping**, then `driver_->exec(...)`.
- `name().display()`: validated by `UserName::parse`
  (`domain/shared/user_name.hpp:37-59`) — alphanumeric + `-_[]` only (the recent
  `[ ]` widening does **not** introduce a quote). No `'`/`;`/`--` possible.
- `locale().text()`: enum-derived fixed strings (safe).
- password hash: hex only via `bn_hash_to_hex`. `command_groups`: digits/commas.
- Verdict: **SAFE-by-validation, but fragile**. Recommend converting to
  `query_bind` anyway (defence in depth; the comment at lines 166-177 already
  acknowledges the upsert is hand-rolled). Not a current vuln.

### B. MySQL account `save` / `find_by_name` — `src/infra/mysql/src/account_repository.cpp:101-109,162-219`
Uses a local `sql_escape()` that doubles single quotes, applied to name/locale/
groups. Escaped (so even a hypothetical quote is neutralized for the standard
case), though hand-rolled rather than bound. Compiled only under
`PVPGN_V3_WITH_MYSQL`. Verdict: **not a clear injection**; recommend
`mysql_real_escape_string` / prepared statements. Note `'` cannot reach it
anyway given UserName validation.

### C. Postgres account `save` / `find_by_name` — `src/infra/postgres/src/account_repository.cpp:100-110,160-251`
Same pattern as MySQL: `sql_escape()` quote-doubling on name/locale/groups, plus
numeric id concatenation. Compiled only under `PVPGN_V3_WITH_POSTGRESQL`.
Verdict: **not a clear injection**; recommend `PQexecParams`. The code comments
themselves say "prefer parameterised queries".

> Note on B/C: these escape, so they are materially safer than Findings 1/2,
> which escape *nothing*. They are listed for completeness/hardening, not as live
> vulns.

---

## SAFE — already fully parameterized (coverage)

All of the following use `query_bind` with `?` placeholders + a params list for
every user-influenced value (string params bound, never concatenated):

| Repo / file | Methods verified safe |
|---|---|
| `account_repository.cpp` | `find_by_id` (`:100`), `find_by_name` (`:128`), `remove` (numeric id only, `:208`), `forEach`/`size` (constant SQL) |
| `clan_repository.cpp` | `load_members` (`:20`), `load_clan`/`find_by_*` — `where_sql` is a hard-coded literal, key is bound (`:50,82,87,92`), `save` (`:109,128`), `remove` (`:162,171`) |
| `friend_list_repository.cpp` | `find_by_owner` (`:21`), `save` (`:52,61`) — all numeric ids bound |
| `ip_ban_repository.cpp` | `is_banned` (`:83,96`), `add_ban` (`:120`), `add_range_ban` (`:137`), `remove_ban`/`remove_range_ban` (`:152,162`), `save_banlist` (`:214`) — ip/reason/network all bound |
| `ladder_repository.cpp` | `get_rank` (`:35,50`), `save_entry` (`:66`), `get_top_n` (`:89`, `kCols` is a constant literal) |
| `realm_repository.cpp` | `find_by_id` (`:42`), `find_by_name` (`:69`), `save` (`:91`), `remove` (`:105`) — name/description bound; `kCols` literal |
| `account_ban_repository.cpp` | `find_active_ban` (`:56`), `add_ban` (`:91`), `remove_ban` (`:105`) — reason bound; `kSelectCols` literal |
| `game_repository.cpp` | `load_players` (`:20`), `load_game`/`find_by_*` (`where_sql` literal, key bound, `:63,83,88`), `save` (`:102,117,126`), `remove` (`:151,160`), `list_active` (`:182`) — name/map/client_tag bound |

In these repos the only strings ever concatenated into the SQL text are
**compile-time literals** (`kCols`, `kSelectCols`, and the hard-coded `where_sql`
fragments like `"id = ?"`); every value originating from data is passed through
the params list.

---

## Recommended remediation order
1. `channel_repository.cpp::save` → convert to `query_bind` (closes Findings 1 & 2). **CRIT, do first.**
2. (Hardening) `account_repository.cpp::save` → `query_bind`.
3. (Hardening) MySQL/Postgres account repos → real prepared/escaped params.
4. (Defence in depth) Have `JoinChannel`/channel creation validate names via a
   charset whitelist, and reject control/quote chars — but treat this as
   secondary to parameterization, never a substitute.
