# 05 · Infrastructure & Persistence

## 1. Goals

* Replace the function-pointer `t_storage` "driver" with a polymorphic
  `IXxxRepository` per aggregate.
* Make all I/O asynchronous and **fiber-aware** so the new event loop
  never blocks the OS thread on a query/disk write.
* Introduce **schema migrations** instead of regenerating tables from
  `sql_DB_layout.conf`.
* Treat configuration as data: typed structs parsed once at boot.
* Replace `eventlog` with structured, levelled, sinkable logging.

## 2. Persistence layout

```
src/infrastructure/persistence/
├── adapter_registry.hpp            # backend factory
├── unit_of_work.hpp / .cpp
├── attribute_map.hpp               # legacy stringly-typed attr bag
│
├── file/                           # legacy flat-file format (parity)
│   ├── file_account_repository.{hpp,cpp}
│   ├── file_clan_repository.{hpp,cpp}
│   ├── file_team_repository.{hpp,cpp}
│   ├── file_ladder_repository.{hpp,cpp}
│   ├── file_layout.{hpp,cpp}       # paths, .save serialization
│   └── flat_db_reader.{hpp,cpp}
│
├── sqlite/                         # primary embedded backend
│   ├── sqlite_account_repository.{hpp,cpp}
│   ├── sqlite_channel_repository.{hpp,cpp}
│   ├── sqlite_clan_repository.{hpp,cpp}
│   ├── sqlite_ladder_repository.{hpp,cpp}
│   ├── sqlite_ban_repository.{hpp,cpp}
│   ├── sqlite_connection.{hpp,cpp} # pooled, fiber-friendly
│   └── statements.hpp              # all SQL constants
│
├── mysql/                          # uses MariaDB connector-cpp,
│   │                               # with mysql_async via Asio
│   └── (mirror of sqlite layout)
│
├── postgres/                       # libpqxx + Asio (preferred over libpq sync)
│   └── (mirror)
│
├── odbc/                           # optional, behind WITH_ODBC
│   └── (mirror)
│
├── memory/                         # for tests
│   ├── in_memory_account_repository.{hpp,cpp}
│   ├── in_memory_channel_repository.{hpp,cpp}
│   ├── …
│
└── migrations/
    ├── 001_initial.sql
    ├── 002_friends_table.sql
    ├── 003_clan_motd.sql
    ├── …
    └── migration_runner.{hpp,cpp}   # discovers, applies, records
```

## 3. Repository pattern

* One `I<Aggregate>Repository` interface per aggregate (defined in
  `application/ports/`).
* Concrete classes live in `infrastructure/persistence/<backend>/`.
* The composition root picks the backend per config:
  ```toml
  [storage]
  driver = "sqlite"                # file | sqlite | mysql | postgres | odbc
  dsn    = "file:pvpgn.db?cache=shared"
  pool   = 4
  ```
* A `RepositoryFactory` returns a tuple of repos. All repos share a
  connection pool object; per-fiber connection checkout is automatic.

### 3.1 Snapshots, not aggregates

Aggregates are not directly mapped to rows. Each repo persists a
**snapshot DTO** (see section 03 §5). The aggregate's
`rehydrate(snapshot)` / `snapshot()` methods are the only mapping
surface.

Benefits:

* Schema changes do not touch the domain layer.
* Backwards-compat reads (e.g. legacy attribute bag) live in the snapshot
  and are massaged by the repo without leaking into domain code.

## 4. Schema migrations

Replace `sql_dbcreator.cpp` (parses `sql_DB_layout.conf` to create
tables) with a versioned, idempotent migration runner:

```
migrations/001_initial.sql            -- creates accounts, friends, clans, etc.
migrations/002_add_ladder_ratings.sql
...
migrations/NNN_xxx.sql
```

`migration_runner` stores the applied set in a `_schema_migrations`
table (id, checksum, applied_at). On startup:

1. Discover migrations on disk.
2. Diff against table.
3. Apply pending in order inside a single transaction per migration.
4. Refuse to start if a previously-applied migration has changed
   (checksum mismatch).

Tooling:
* `pvpgn-db-tool migrate --target N` for ops.
* CI matrix runs migrations on SQLite/MySQL/Postgres for every PR.

## 5. Fiber-aware DB connections

Synchronous client libraries (libsqlite3, libpq, libmysqlclient) block
the calling OS thread.  Two layers solve this:

* **SQLite**: wrap each call in `boost::asio::post(io_ctx,
  use_future)` on a small thread-pool dedicated to DB; fiber awaiters
  yield until the future fires.
* **MySQL/Postgres**: use the async APIs (`mysql_real_query_nonblocking`
  / `PQsendQuery`) integrated with Asio reactors; fiber yields on the
  socket's `wait_for` events.

The fiber API surface looks synchronous:

```cpp
auto rows = sqliteConn.exec("SELECT id FROM accounts WHERE name = ?", name);
```

…but underneath the fiber yields if the operation cannot complete
immediately. No callback hell, no per-DB-call thread.

## 6. Configuration

* Replace the hand-written conf parser (`common/conf.cpp`) with a typed
  loader on top of **toml++** (preserving INI files via a compatibility
  shim during the migration window — current `.conf` syntax already
  resembles INI).
* Config is parsed once into immutable typed structs:

```cpp
struct ServerConfig {
    std::string  bindAddr   = "0.0.0.0";
    std::uint16_t port      = 6112;
    std::string  servername = "PvPGN";
    std::filesystem::path scriptDir;
    LogConfig    log;
    StorageConfig storage;
    LuaConfig    lua;
    WebApiConfig webapi;
    MetricsConfig metrics;
    // …
};
tl::expected<ServerConfig, ConfigError> load_server_config(const std::filesystem::path&);
```

* Hot-reload: `SIGHUP` triggers `ConfigWatcher::reload()` which produces
  a new `ServerConfig` and feeds it to `IConfigSubscriber`s; subsystems
  decide what is safe to rebind.
* `bnetd_default_user.plain.in` and other `*.in` files go through
  `configure_file` only for build-time defaults (paths). Runtime config
  is no longer regenerated by CMake.

## 7. Logging

* **`spdlog`** (already in scope via `fmt`) replaces `eventlog`.
* Levels: `trace, debug, info, warn, error, critical` (close to current).
* Multiple sinks: rotating file (current), stdout (systemd), syslog,
  JSON-line file (for log aggregators), Windows Event Log.
* Mandatory contextual fields per log entry:

  ```
  account_id, session_id, client_tag, ip, module
  ```

  attached via `spdlog::mdc` so handlers don't have to thread them
  manually like `__FUNCTION__` is today.

* `eventlog_level_*` constants are removed; macros `LOG_INFO`/`LOG_DEBUG`
  in `core/logging.hpp` wrap `spdlog::info`/`debug` so call sites are
  trivial.

## 8. Clock / time

* `IClock` interface (`now()`, `monotonic()`, `wallTime()`).
* `SystemClock` (production) reads `std::chrono::system_clock::now()`.
* `ManualClock` (tests) advances explicitly.
* Eliminates `extern time_t now;` and the `setitimer` SIGALRM trick in
  `server.cpp`.

## 9. Scheduler / timers

* `IScheduler::scheduleAfter(Duration, Callable)` returns a
  `TimerHandle`.
* Backed by `boost::asio::steady_timer` (one per scheduled task) or a
  single timing-wheel for high-frequency timers.
* Replaces the linked-list of `t_timer` + `timer_check()` polling each
  loop iteration in `bnetd/timer.cpp`.

## 10. Filesystem and path handling

* All path manipulation through `std::filesystem`.
* No more `char path[256]; snprintf(path, sizeof(path), "%s/%s", a, b);`
  patterns.

## 11. Scripting (host side of infra; details in section 10)

* `infrastructure/scripting/lua/` hosts a sol3-based engine.
* Scripts subscribe to **domain events** (typed bindings) and call
  whitelisted **use-cases** through generated bindings.
* Existing Lua scripts in `lua/` continue to work via a compat shim
  that maps the old `t_account`/`t_connection` lua tables onto the new
  view objects.

## 12. Removing legacy infra modules

| Legacy module | Replacement |
|---|---|
| `common/xalloc.*`              | `std::make_unique`, `std::vector` |
| `common/list.*`, `elist.h`     | `std::vector`, `boost::intrusive::list` |
| `common/hashtable.*`           | `absl::flat_hash_map` or `std::unordered_map` |
| `common/bn_type.*`             | `boost::endian` / `std::endian` |
| `common/fdwatch*.*`            | Boost.Asio |
| `common/xstr.*`,`xstring.*`    | `std::string`, `fmt::format`, `std::string_view` |
| `common/conf.*`                | `toml++` typed config |
| `common/pugixml.*`             | Drop (only one config user); use `toml++` or JSON |
| `common/asnprintf.*`           | `fmt::format` |
| `common/tag.*`,`token.*`       | Move into `domain/shared` or `core` |
| `common/scoped_array.h`,`scoped_ptr.h` | `std::unique_ptr` |
| `compat/*` (`gettimeofday`, `strdup`, `pgetopt`, …) | Drop entirely (C++11/17 stdlib already provides) |
