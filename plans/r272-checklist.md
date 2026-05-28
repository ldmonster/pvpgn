# R272 — Fix MySQL/PostgreSQL Stub Signature Drift

## Status: COMPLETE

## Files Modified

### MySQL (`src/v3/infra/mysql/`)
- `include/infra/mysql/unit_of_work.hpp` — Added missing `[[nodiscard]] ITeamRepository& teams() override`; updated TODO comment to include TeamRepository
- `include/infra/mysql/account_repository.hpp` — Replaced all 6 stale method signatures with the 7 correct port signatures matching `IAccountRepository`

### PostgreSQL (`src/v3/infra/postgres/`)
- `include/infra/postgres/unit_of_work.hpp` — Added missing `[[nodiscard]] ITeamRepository& teams() override`; updated TODO comment to include TeamRepository
- `include/infra/postgres/account_repository.hpp` — Replaced all 6 stale method signatures with the 7 correct port signatures matching `IAccountRepository`; added missing `<cstdint>`, `<string_view>`, `<vector>` includes

## Drift Found

### 1. `MySQLUnitOfWork` / `PostgreSQLUnitOfWork` — Missing `teams()` override
- **Port** (`IUnitOfWork`): `[[nodiscard]] virtual ITeamRepository& teams() = 0;` (added in R269)
- **Stubs**: declared 9 repository accessors (`accounts` through `realms`) but omitted `teams()`
- **Fix**: Added `[[nodiscard]] application::ports::ITeamRepository& teams() override;` to both classes

### 2. `MySQLAccountRepository` / `PostgreSQLAccountRepository` — Completely wrong method signatures
- **Port** (`IAccountRepository`) declares:
  - `find_by_name(std::string_view name)`
  - `find_by_id(uint32_t id)`
  - `save(const domain::identity::Account&)`
  - `remove(std::string_view name)`
  - `exists(std::string_view name)`
  - `list_online()`
  - `count()`
- **Stubs** declared a different, non-matching API:
  - `get_by_id(domain::AccountId id)` — wrong name, wrong type
  - `get_by_username(std::string_view username)` — wrong name
  - `create(const domain::Account&)` — wrong name, wrong type
  - `update(const domain::Account&)` — wrong name, wrong type
  - `delete_account(domain::AccountId id)` — wrong name, wrong type
  - `get_all()` — wrong name, wrong return type
  - Missing: `exists()`, `list_online()`, `count()`
- **Fix**: Replaced all 6 stale declarations with the 7 correct port-matching signatures using `domain::identity::Account` and `uint32_t id`

## Notes
- No `.cpp` source files exist for MySQL or PostgreSQL stubs (headers only); no `.cpp` changes were needed
- The `MySQLUnitOfWorkFactory` and `PostgreSQLUnitOfWorkFactory` headers were already correct — `create()` override matches `IUnitOfWorkFactory`
- The `MySQLConnection` and `PostgreSQLConnection` headers are not port implementations and required no changes
- These are stubs — no method bodies were added; implementations remain as `= 0` overrides to be filled in when real SQL backends are built
- The file adapter (`src/v3/infra/file/`) was not touched (out of scope)
- The InMemory adapter was not touched (reference implementation, already correct)
