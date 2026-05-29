# R317 — Fix static-local data race in UoW `channels()`/`games()`

## Checklist

### SQLiteUnitOfWork (`src/v3/infra/sqlite/`)
- [x] Read `src/v3/infra/sqlite/include/infra/sqlite/unit_of_work.hpp`
- [x] Read `src/v3/infra/sqlite/src/unit_of_work.cpp`
- [x] Add `std::unique_ptr<inmemory::InMemoryChannelRepository> channels_` member field
- [x] Add `std::unique_ptr<inmemory::InMemoryGameRepository> games_` member field
- [x] Add `std::unique_ptr<inmemory::InMemoryTeamRepository> teams_` member field (already not persisted to SQL)
- [x] Initialize `channels_` in constructor: `std::make_unique<inmemory::InMemoryChannelRepository>()`
- [x] Initialize `games_` in constructor: `std::make_unique<inmemory::InMemoryGameRepository>()`
- [x] Initialize `teams_` in constructor: `std::make_unique<inmemory::InMemoryTeamRepository>()`
- [x] Change `channels()` to return `*channels_` (no static local)
- [x] Change `games()` to return `*games_` (no static local)
- [x] Change `teams()` to return `*teams_` (no static local)
- [x] Include `infra/inmemory/channel_repository.hpp` in header
- [x] Include `infra/inmemory/game_repository.hpp` in header
- [x] Include `infra/inmemory/in_memory_team_repository.hpp` in header

### FileUnitOfWork (`src/v3/infra/file/`)
- [x] Read `src/v3/infra/file/include/infra/file/unit_of_work.hpp`
- [x] Read `src/v3/infra/file/src/unit_of_work.cpp`
- [x] Add `std::unique_ptr<inmemory::InMemoryChannelRepository> channels_` member field
- [x] Add `std::unique_ptr<inmemory::InMemoryGameRepository> games_` member field
- [x] Add `std::unique_ptr<inmemory::InMemoryClanRepository> clans_` member field
- [x] Add `std::unique_ptr<inmemory::InMemoryLadderRepository> ladder_` member field
- [x] Add `std::unique_ptr<inmemory::InMemoryAccountBanRepository> account_bans_` member field
- [x] Add `std::unique_ptr<inmemory::InMemoryFriendListRepository> friend_lists_` member field
- [x] Add `std::unique_ptr<inmemory::InMemoryRealmRepository> realms_` member field
- [x] Add `std::unique_ptr<inmemory::InMemoryTeamRepository> teams_` member field
- [x] Initialize all per-instance repos in constructor with `std::make_unique<>()`
- [x] Change `channels()` to return `*channels_` (no static local)
- [x] Change `games()` to return `*games_` (no static local)
- [x] All other accessors also return per-instance members

### Verification
- [x] No `static` keyword appears in any `channels()` or `games()` method body
- [x] Each `UnitOfWork` instance owns its own repository objects
- [x] No shared mutable state between UoW instances
- [x] `InMemoryChannelRepository` used as placeholder (no `SqliteChannelRepository` exists yet)
- [x] `InMemoryGameRepository` used as placeholder (no `SqliteGameRepository` exists yet)

## Result
The static-local data race in `channels()` and `games()` is fully fixed in both
`SQLiteUnitOfWork` and `FileUnitOfWork`:

- **`SQLiteUnitOfWork`** (`src/v3/infra/sqlite/`): `channels_`, `games_`, and `teams_` are
  `std::unique_ptr` member fields initialized in the constructor. The accessor methods
  `channels()`, `games()`, and `teams()` return `*channels_`, `*games_`, and `*teams_`
  respectively — no `static` locals.

- **`FileUnitOfWork`** (`src/v3/infra/file/`): All session-scoped repositories (`channels_`,
  `games_`, `clans_`, `ladder_`, `account_bans_`, `friend_lists_`, `realms_`, `teams_`) are
  per-instance `std::unique_ptr` members initialized in the constructor. Every accessor
  returns a reference to its own instance member.

Since `SqliteChannelRepository` and `SqliteGameRepository` do not yet exist,
`InMemoryChannelRepository` and `InMemoryGameRepository` are used as placeholders,
consistent with the pattern already established for `teams_` in R312.
