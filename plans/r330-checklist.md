# R330 Checklist — Wire BnetdService to Configurable Persistence Backend

## Goal
Replace the hardcoded `InMemoryUnitOfWorkFactory` in `main.cpp` with a
configurable backend selected from the `[persistence]` section of `bnetd.toml`.
Default backend is `"sqlite"`.

## Tasks

- [x] Add `PersistenceConfig` struct to `src/v3/infra/config/include/infra/config/server_config.hpp`
      — Fields: `backend` (string, default `"sqlite"`) and `dsn` (string)
- [x] Add `persistence` member to `ServerConfig` struct
- [x] Add `parse_persistence()` function to `src/v3/infra/config/src/server_config.cpp`
      — Reads `[persistence]` TOML section → `sc.persistence.backend` and `sc.persistence.dsn`
- [x] Call `parse_persistence()` in `from_config()` (before `parse_storage`)
- [x] Add `[persistence]` section to `conf/bnetd.toml.in`
      — `backend = "sqlite"` (default)
      — `dsn = "${LOCALSTATEDIR}/pvpgn.db"`
      — Documents all four backends with example DSN formats
- [x] Add includes to `src/v3/app/bnetd/src/main.cpp`
      — `#include "infra/sqlite/unit_of_work_factory.hpp"` (always)
      — `#include "infra/mysql/unit_of_work_factory.hpp"` (via `__has_include`)
      — `#include "infra/postgres/unit_of_work_factory.hpp"` (via `__has_include`)
- [x] Replace hardcoded `InMemoryUnitOfWorkFactory` in `main()` with backend dispatch:
      — Read `persistence_backend` and `persistence_dsn` from loaded TOML config
      — `"inmemory"` → `InMemoryUnitOfWorkFactory`
      — `"mysql"` → `MySQLUnitOfWorkFactory(dsn)` (guarded by `PVPGN_V3_WITH_MYSQL`)
      — `"postgres"` → `PostgreSQLUnitOfWorkFactory(dsn)` (guarded by `PVPGN_V3_WITH_POSTGRESQL`)
      — default/`"sqlite"` → `SQLiteUnitOfWorkFactory(dsn)`
      — Throws `std::runtime_error` if backend requested but not compiled in
- [x] Add optional link dependencies to `src/v3/app/bnetd/CMakeLists.txt`
      — `pvpgn_infra_sqlite`, `pvpgn_infra_mysql`, `pvpgn_infra_postgresql`
      — All guarded by `if(TARGET …)`

## Files Changed
| File | Change |
|------|--------|
| `src/v3/infra/config/include/infra/config/server_config.hpp` | Added `PersistenceConfig` struct + `persistence` field |
| `src/v3/infra/config/src/server_config.cpp` | Added `parse_persistence()` + call in `from_config()` |
| `conf/bnetd.toml.in` | Added `[persistence]` section |
| `src/v3/app/bnetd/src/main.cpp` | Added backend includes + configurable factory dispatch |
| `src/v3/app/bnetd/CMakeLists.txt` | Added optional link deps for sqlite/mysql/postgres |

## Backend Selection Logic
```
[persistence]
backend = "sqlite"          # or "inmemory", "mysql", "postgres"
dsn     = "/var/pvpgn.db"   # path for sqlite; "host:port:user:pass:db" for mysql/postgres
```

| backend    | DSN format                          | Compile guard              |
|------------|-------------------------------------|----------------------------|
| `inmemory` | (ignored)                           | always available           |
| `sqlite`   | filesystem path to `.db` file       | always available           |
| `mysql`    | `host:port:user:password:database`  | `PVPGN_V3_WITH_MYSQL`      |
| `postgres` | `host:port:user:password:database`  | `PVPGN_V3_WITH_POSTGRESQL` |

## Notes
- Config loading is gated on `PVPGN_V3_BNETD_HAVE_LOGGER_FACTORY` (same guard as
  the existing logger init block) — falls back to `"sqlite"` + `"pvpgn.db"` when
  no config file is provided.
- The `owned_uow_factory` (`unique_ptr`) keeps the factory alive for the server
  lifetime; `uow_factory` is a reference to it for passing to `BnetdService`.
