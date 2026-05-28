# R269 — Wire ITeamRepository into IUnitOfWork

## Status: COMPLETE

## Files Modified
- `src/v3/application/ports/include/application/ports/unit_of_work.hpp` — added `ITeamRepository` forward declaration and `[[nodiscard]] virtual ITeamRepository& teams() = 0;` pure virtual accessor
- `src/v3/infra/inmemory/include/infra/inmemory/unit_of_work.hpp` — added `#include "infra/inmemory/in_memory_team_repository.hpp"`, `std::shared_ptr<InMemoryTeamRepository> teams_` constructor parameter + member, and `ITeamRepository& teams() override` implementation
- `src/v3/infra/inmemory/include/infra/inmemory/unit_of_work_factory.hpp` — added `InMemoryTeamRepository` forward declaration and `std::shared_ptr<InMemoryTeamRepository> teams_` member
- `src/v3/infra/inmemory/src/unit_of_work_factory.cpp` — added `#include "infra/inmemory/in_memory_team_repository.hpp"`, initialised `teams_` in constructor, and passed `teams_` to `InMemoryUnitOfWork::create()`

## Notes
- `InMemoryUnitOfWork` uses shared_ptr injection (not default-constructed members), so the factory required updates to construct and pass the new `InMemoryTeamRepository` instance alongside the existing repositories.
- The `teams_` shared_ptr is owned by the factory and shared across all `InMemoryUnitOfWork` instances, consistent with the pattern used for all other repositories.
- No changes were needed to `IUnitOfWorkFactory` (port interface) — it remains unchanged.
