# R248 — StatusCode completeness + from_errno()

## Checklist

- [x] Read current `src/v3/core/include/core/error.hpp`
- [x] Added `Conflict = 17` to `StatusCode` enum
- [x] Added `RateLimited = 18` to `StatusCode` enum
- [x] Added `Timeout = 19` to `StatusCode` enum
- [x] Added `ProtocolError = 20` to `StatusCode` enum
- [x] Added `NetworkError = 21` to `StatusCode` enum
- [x] Added `ConfigError = 22` to `StatusCode` enum
- [x] Added `DependencyFailed = 23` to `StatusCode` enum
- [x] Added `SchemaMismatch = 24` to `StatusCode` enum
- [x] Added `from_errno(int) -> StatusCode` declaration in header
- [x] Implemented `from_errno()` with full POSIX errno mapping
- [x] Updated `to_string(StatusCode)` to cover all 25 codes
- [x] CMakeLists.txt updated if new .cpp file created
- [x] All existing tests still compile (no breaking changes)

## Exit Criterion

`grep -c 'StatusCode::' src/v3/core/include/core/error.hpp` returns ≥ 25 (Ok through SchemaMismatch).
`from_errno(ETIMEDOUT)` returns `StatusCode::Timeout`.

## Status: GREEN
