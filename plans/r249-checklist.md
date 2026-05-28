# R249 — PVPGN_VERIFY contract macro

## Checklist

- [x] Created `src/v3/core/include/core/contract.hpp`
- [x] `PVPGN_VERIFY(cond, fmt, ...)` macro defined
- [x] Debug mode: logs CRITICAL + calls `std::abort()`
- [x] Release mode: logs CRITICAL + calls `std::terminate()`
- [x] `pvpgn::core::detail::verify_fail()` helper is `[[noreturn]]`
- [x] Header selfcheck target added to `tests/unit/core/CMakeLists.txt`
- [x] Unit tests created in `tests/unit/core/contract_test.cpp`
- [x] Tests registered in `tests/unit/core/CMakeLists.txt`
- [x] No existing files modified except `tests/unit/core/CMakeLists.txt`

## Exit Criterion

`PVPGN_VERIFY(true, "ok")` compiles and runs without side effects.
`PVPGN_VERIFY(false, "fail {}", 42)` would log CRITICAL and abort/terminate.

## Status: GREEN
