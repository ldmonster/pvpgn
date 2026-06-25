# Concurrency bug-hunt — data layer (in-memory repos, persistence UoW, event bus, session registry, metrics)

Subsystem: in-memory repositories + shared domain state + the persistence Unit-of-Work, accessed concurrently by multiple connection-handler threads.

## Threading model (why these races are live, not theoretical)

`src/app/bnetd/src/main.cpp:466-477` runs the Asio runtime with

```cpp
const std::size_t n_threads =
    cfg.worker_threads > 0 ? cfg.worker_threads
                           : std::max(1u, std::thread::hardware_concurrency());
rt.run(n_threads);          // install_fiber_scheduler defaults to FALSE
```

`IoRuntime::run` (`src/infra/net/src/io_runtime.cpp:25-55`) spawns **N OS worker
threads, each calling `ctx_.run()`**, when the fiber scheduler is NOT installed
(the default in bnetd). Connection handlers therefore execute **concurrently on
multiple OS threads**. (Only when `install_fiber_scheduler==true` is the pool
coerced to a single thread; bnetd does not do that.)

The shared state is constructed once in `main()` and shared by reference across
every handler thread:
- `src/app/bnetd/src/main.cpp:340` `InMemorySessionRegistry session_reg;`
- `src/app/bnetd/src/main.cpp:342` `InMemoryEventBus event_bus;`
- `src/app/bnetd/src/main.cpp:339,341,343` channel/game/ip-ban repos
- the single `SQLiteUnitOfWorkFactory` (`main.cpp:323`) whose `create()` is
  invoked per request from those threads.

So every "no lock" finding below is a genuine multi-threaded data race under the
default deployment.

---

## CRITICAL

### C1 — One `SQLiteConnection` (`sqlite3*`) shared across all threads/UoWs; the connection has no synchronization

- Severity: **CRITICAL** (memory corruption / `SQLITE_MISUSE` / DB corruption).
- v3 refs:
  - `src/infra/sqlite/src/unit_of_work_factory.cpp:13-47` — the factory opens
    **one** `conn_ = std::make_shared<SQLiteConnection>(...)` and **every**
    `create()` returns `std::make_unique<SQLiteUnitOfWork>(conn_)` over that
    same connection.
  - `src/infra/sqlite/src/unit_of_work.cpp:20-34` — each UoW builds a
    `SqliteDriver` and all SQL repositories over the shared `conn_`.
  - `src/infra/sqlite/include/infra/sqlite/connection.hpp:6-8` self-documents:
    "synchronous, **single-threaded** access… For multi-threaded access, create
    one connection per thread."
  - `src/infra/sqlite/src/connection.cpp` — `SQLiteConnection` has **no mutex**.
    `query`/`query_bind` (`connection.cpp:94-179`) do
    `sqlite3_prepare_v2` → loop `sqlite3_step` → `sqlite3_finalize` on the shared
    `db_` with no serialization; `exec`/`begin`/`commit`/`rollback` likewise.
  - `sqlite3_open` (`connection.cpp:36`) uses the default mode. Even if SQLite
    were compiled in serialized threading mode (mutex per call), the
    **statement-level** sequence here is not atomic, and the driver's
    `tx_depth_`/`in_transaction_` flags are plain `int`/`bool`.
- Racing interleaving: thread A `SqlAccountRepository::save` is mid
  `sqlite3_step` on a prepared stmt; thread B (another connection handler) runs
  `SqlClanRepository::find_by_*` which calls `sqlite3_prepare_v2`/`step` on the
  same `db_`. Concurrent use of a single `sqlite3*` handle is undefined behavior
  unless the handle is in serialized mode AND callers serialize multi-step
  sequences — neither holds. Result: `SQLITE_MISUSE`, interleaved/garbled rows,
  use-after-free of stmt state, or on-disk corruption.
- Transaction corruption sub-case: `SqliteDriver::tx_depth_`
  (`sqlite_driver.cpp:112-175`) and `SQLiteConnection::in_transaction_` are
  per-`SqliteDriver`/per-connection but a single physical SQLite transaction is
  global to the connection. Two UoWs over the shared connection each call
  `begin_transaction()` → both emit `BEGIN`/`SAVEPOINT` against the same handle.
  Thread B's `BEGIN` fails ("cannot start a transaction within a transaction")
  or, worse, thread B's `COMMIT` commits thread A's uncommitted writes. The
  SAVEPOINT nesting in `sqlite_driver.cpp` is per-driver depth and assumes the
  driver owns the connection exclusively.
- Fix (any of):
  1. **One connection per UoW** — have `SQLiteUnitOfWorkFactory::create()` open
     a fresh `SQLiteConnection` (one per request/thread). This is what the
     header doc prescribes. Migrations stay on the factory's bootstrap conn.
  2. Connection-per-thread pool keyed by `std::this_thread::get_id()`.
  3. If a single shared connection must remain, add a `std::recursive_mutex` to
     `SQLiteConnection` (or to `SqliteDriver`) and hold it across each whole
     UoW transaction (begin…commit), not merely per statement — plus open with
     `SQLITE_OPEN_FULLMUTEX`. Option 1 is strongly preferred; SQLite write
     concurrency on one handle serializes anyway.

Note: the MySQL/Postgres UoW factories (`main.cpp:303,312`) should be audited
for the same shared-handle pattern; out of scope here but likely identical.

---

## HIGH

### H1 — `InMemorySessionRegistry` has NO mutex; concurrent login/logout corrupts the bidirectional maps

- Severity: **HIGH** (data race → map corruption, lost/duplicate sessions,
  crash).
- v3 ref: `src/infra/inmemory/session_registry.hpp:18-73` — the class has two
  `std::unordered_map` members (`account_for_session_`, `session_for_account_`)
  and **not a single lock**. `attach` (l.21-36) mutates both maps; `detach`
  (l.38-43) erases from both; `session_for`/`account_for` (l.45-57) read;
  `list` (l.59-68) iterates.
- Usage proving concurrency: injected as a shared singleton
  (`main.cpp:340`) into `LoginUser`/`LogoutUser`
  (`src/application/auth/src/login_user.cpp:62,69`,
  `logout_user.cpp:47`) and the chat use-cases
  (`post_message.cpp:54`, `join_channel.cpp:87`, `leave_channel.cpp:30`,
  `kick_connection.cpp:19`) — all on connection-handler threads.
- Racing interleaving: thread A `attach(s1,a1)` writes `account_for_session_`
  while thread B `attach(s2,a2)` rehashes the same bucket array → torn
  pointers/UB; or A `detach` erases while B `session_for` reads the same node →
  use-after-free. Also the check-then-act in `attach` (`contains` then
  `operator[]`) is non-atomic: two threads logging the same account in can both
  pass the `session_for_account_.contains` guard and double-insert.
- Fix: add `mutable std::shared_mutex mutex_`; `unique_lock` in
  `attach`/`detach`, `shared_lock` in `session_for`/`account_for`/`list`. The
  check-then-act in `attach` is then atomic under the unique lock.

### H2 — `InMemoryEventBus` publish/subscribe/unsubscribe are not thread-safe

- Severity: **HIGH** (data race on the handler map → crash / lost or
  double-invoked handlers).
- v3 ref: `src/infra/inmemory/event_bus.hpp:16-46`. Only `next_id_` is atomic.
  `subscribe` (l.31-36) does `handlers_.emplace(...)`, `unsubscribe` (l.38-40)
  does `handlers_.erase(...)`, and `publish` (l.18-29) copies the map
  (`const auto snapshot = handlers_;`) — **all unguarded**. The header even
  claims "single-threaded" (l.5-6), but it is wired as a shared singleton
  (`main.cpp:342`) used from handler threads.
- Racing interleaving: thread A `publish` copy-constructs `snapshot` from
  `handlers_` while thread B `subscribe` is mid-`emplace` (rehash) → reads a
  half-updated bucket array → crash. Or A `unsubscribe` erases while B `publish`
  copies → UB.
- Good part: copying the map before invoking handlers correctly avoids
  iterator-invalidation/re-entrancy *if* the copy itself were atomic — handlers
  are not invoked under a lock, so a handler that re-enters `subscribe`/`publish`
  won't self-deadlock. The only defect is the missing lock around the map
  mutations and the snapshot copy.
- Fix: add a `std::mutex`; lock around `emplace`/`erase` and around taking the
  `snapshot` copy in `publish`; release before invoking handlers (preserves the
  current non-reentrant-deadlock behavior). The `subscribe`-during-`publish`
  semantics already match the snapshot contract.

---

## Correctly synchronized (coverage — no action needed)

These return **by value** or by **`shared_ptr`/`unique_ptr`-copied value** and
lock every method that touches the container, with `shared_lock` for reads and
`unique_lock` for writes. No reference/pointer/iterator into the container
escapes the critical section:

- `InMemoryAccountRepository` (`account_repository.hpp:24-98`) — `find_by_*`
  return `core::Result<Account>` **by value** (`return *it->second;` copies),
  `forEach`/`size` under `shared_lock`, writes under `unique_lock`. Safe.
- `InMemoryChannelRepository` (`channel_repository.hpp`) — same pattern,
  returns `Channel` by value. Safe.
- `InMemoryGameRepository` (`game_repository.hpp`) — returns `shared_ptr<Game>`
  (safe: a copy of the owning pointer survives a later `remove`); all methods
  locked. Read methods are non-`const` but still take `shared_lock`. Safe.
- `InMemoryClanRepository` (`clan_repository.hpp`) — returns `shared_ptr<Clan>`;
  all methods locked; `find_by_name` does a locked linear scan. Safe.
- `InMemoryFriendListRepository` (`friend_list_repository.hpp`) — returns
  `FriendList` by value; locked. Safe.
- `InMemoryTeamRepository` (`in_memory_team_repository.hpp`) — returns
  `shared_ptr<Team>` / vector of them; `next_id` is `shared_lock` (read-only
  scan — fine since callers serialize id allocation through `save`). Safe re:
  data race.
- `InMemoryIpBanRepository` (`ip_ban_repository.hpp`) — wraps `IpBanList`;
  every accessor locked, `for_each_entry`/`load_banlist`/`is_banned` under
  `shared_lock`, mutators under `unique_lock`; returns the list **by value**.
  Safe.
- `InMemoryLadderRepository` (`ladder_repository.hpp`) — returns vectors / rank
  **by value**; locked. Safe.
- `InMemoryMetricsRegistry` (`in_memory_metrics_registry.{hpp,cpp}`) —
  `metrics_mu_` (`shared_mutex`): `counter`/`gauge`/`histogram`
  lookup-or-create under `unique_lock` (atomic check-then-create, no double
  registration), `serialize` under `shared_lock`. Counters/gauges use atomic
  CAS; histogram bucket counts are atomics. Safe.
- `IoRuntime` (`io_runtime.{hpp,cpp}`) — start/stop guarded by
  `std::atomic<bool> running_` CAS; `request_stop` is signal/worker-safe and
  documented not to self-join. Safe.

## forEach / callback-under-lock note (informational, low risk)

`InMemoryAccountRepository::forEach`, `InMemoryChannelRepository::forEach`,
`InMemoryIpBanRepository::for_each_entry`, and the locked linear scans in
`InMemoryClanRepository::find_by_name`/`InMemoryTeamRepository::find_by_member`
invoke a user callback **while holding `shared_lock`**. This is correct against
data races (no other thread can mutate mid-iteration), but it means a callback
that re-enters the **same** repo with a *write* (`save`/`remove`, which take
`unique_lock`) would **deadlock** (`shared_mutex` is not recursive, and a writer
waiting behind the held reader self-deadlocks). Current call sites pass
read-only predicates, so this is latent, not active. Worth a code comment /
contract note that `forEach` predicates must not call back into the repository.

---

## Summary

- **CRITICAL: 1** — C1 shared `SQLiteConnection` across threads/UoWs (no
  synchronization; transaction-flag corruption).
- **HIGH: 2** — H1 `InMemorySessionRegistry` unlocked; H2 `InMemoryEventBus`
  unlocked.
- 11 components verified correctly synchronized (8 in-memory repos + metrics
  registry + IoRuntime, all return-by-value/shared_ptr with full locking).
- 1 informational: `forEach`/scan invoke callbacks under the read lock —
  re-entrant writes would deadlock; document the contract.
