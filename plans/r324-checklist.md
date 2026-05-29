# R324 Checklist — Shadow-write infrastructure + feature flag

## Goal
Implement a `ShadowAccountRepository` adapter that wraps a primary and secondary
`IAccountRepository`. All reads go to the primary; writes go to both (secondary
is best-effort when the feature flag is enabled). Also provide matching
`ShadowUnitOfWork` / `ShadowUnitOfWorkFactory` so the composition root can
wire the shadow layer without touching use-case code.

## Files created

- [x] `src/v3/infra/shadow/include/infra/shadow/shadow_account_repository.hpp`
- [x] `src/v3/infra/shadow/src/shadow_account_repository.cpp`
- [x] `src/v3/infra/shadow/include/infra/shadow/shadow_unit_of_work.hpp`
- [x] `src/v3/infra/shadow/src/shadow_unit_of_work.cpp`
- [x] `src/v3/infra/shadow/include/infra/shadow/shadow_unit_of_work_factory.hpp`
- [x] `src/v3/infra/shadow/src/shadow_unit_of_work_factory.cpp`
- [x] `src/v3/infra/shadow/CMakeLists.txt`

## Files modified

- [x] `src/v3/CMakeLists.txt` — added `add_subdirectory(infra/shadow)`

## Design notes

- `ShadowAccountRepository` holds references to `primary_` and `secondary_`
  plus a `bool enabled_` feature flag.
- Read operations (`find_by_id`, `find_by_name`, `forEach`, `size`) always
  delegate to `primary_` only.
- Write operations (`save`, `remove`) write to `primary_` first; if `enabled_`
  is true, the same write is mirrored to `secondary_` best-effort (errors
  ignored so the primary path is never blocked).
- `ShadowUnitOfWork` wraps two `IUnitOfWork&` references; `commit()` commits
  primary first, then secondary best-effort.
- `OwningShadowUnitOfWork` owns two `unique_ptr<IUnitOfWork>` for factory use.
- `ShadowUnitOfWorkFactory` creates `OwningShadowUnitOfWork` instances from
  two `IUnitOfWorkFactory&` references.

## Status: DONE
