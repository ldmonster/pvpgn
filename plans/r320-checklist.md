# R320 — `channels` table + `SqliteChannelRepository`

## Checklist
- [x] Read `IChannelRepository` port interface (`channel_repository.hpp`)
- [x] Read `domain::chat::Channel` aggregate (`channel.hpp`)
- [x] Read existing SQLite repository pattern (`account_repository.hpp` / `.cpp`)
- [x] Read `SQLiteUnitOfWork` header and source
- [x] Read existing migration files (`001_initial_schema.sql`, `all_migrations.cpp`)
- [x] Created `src/v3/infra/migrations/sql/002_channels.sql` with `channels` table schema
- [x] Registered migration 002 in `src/v3/infra/migrations/src/all_migrations.cpp` (embedded SQL + updated `all_migrations_array` from size 1 to 2)
- [x] Created `src/v3/infra/sqlite/include/infra/sqlite/sqlite_channel_repository.hpp`
- [x] Created `src/v3/infra/sqlite/src/sqlite_channel_repository.cpp` implementing all `IChannelRepository` methods: `find_by_name()`, `find_by_id()`, `save()`, `remove()`, `forEach()`, `size()`
- [x] Updated `src/v3/infra/sqlite/include/infra/sqlite/unit_of_work.hpp` — replaced `InMemoryChannelRepository` include/member with `SqliteChannelRepository`
- [x] Updated `src/v3/infra/sqlite/src/unit_of_work.cpp` — replaced `InMemoryChannelRepository` construction with `SqliteChannelRepository(conn_)`
- [x] Updated `src/v3/infra/sqlite/CMakeLists.txt` — added `sqlite_channel_repository.hpp` and `sqlite_channel_repository.cpp`
- [x] `pvpgn_infra_migrations` already linked in `CMakeLists.txt` — no additional link change needed

## Result
Created a full SQLite-backed `SqliteChannelRepository` that implements `IChannelRepository`. The `channels` table is created by migration 002 (registered in `all_migrations.cpp`). The repository persists channel name, topic, flags (as a bitmask integer), and max_members. `Channel` aggregates are rehydrated via `Channel::rehydrate()` with empty member/banlist maps (session-scoped data is not persisted). `SQLiteUnitOfWork` now uses `SqliteChannelRepository` instead of the previous `InMemoryChannelRepository` placeholder.
