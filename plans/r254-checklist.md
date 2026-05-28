# R254 — Fix realm/character clock injection

## Checklist

- [x] Read `domain/realm/character.hpp` and `.cpp`
- [x] Read `core/clock.hpp` to understand `core::SystemTime`
- [x] Changed `Character` constructor to accept `core::SystemTime now` parameter
- [x] Changed `touch()` to accept `core::SystemTime now` parameter
- [x] Changed stored member types from `system_clock::time_point` to `core::SystemTime`
- [x] Removed direct `std::chrono::system_clock::now()` calls from domain code
- [x] Updated all callers to pass `core::SystemTime`
- [x] Updated existing tests to use fixed time for determinism
- [x] No `system_clock::now()` calls remain in `src/v3/domain/`

## Exit Criterion

`grep -r 'system_clock::now' src/v3/domain/` returns zero results.
`Character` constructor compiles with an injected `core::SystemTime` parameter.

## Status: GREEN
