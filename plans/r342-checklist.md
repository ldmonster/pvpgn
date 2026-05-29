# R342 — CMake Warning/Error Helpers

## Status: COMPLETE

## What was done

### New file: `cmake/v3_warnings.cmake`
- Created `pvpgn_v3_target_warnings(target)` macro:
  - GCC/Clang: `-Wall -Wextra -Wpedantic -Wshadow -Wnon-virtual-dtor -Wold-style-cast -Wcast-align -Wunused -Woverloaded-virtual -Wconversion -Wsign-conversion -Wnull-dereference -Wdouble-promotion -Wformat=2 -Wimplicit-fallthrough -Wno-maybe-uninitialized`
  - MSVC: `/W4 /permissive-`
- Created `pvpgn_v3_target_werror(target)` macro:
  - GCC/Clang: `-Werror`
  - MSVC: `/WX`
- All flags applied via `target_compile_options(... PRIVATE ...)` — never `add_compile_options()`
- `include_guard(GLOBAL)` prevents double-inclusion

### Modified file: `cmake/v3.cmake`
- Added `include(v3_warnings)` immediately after `include_guard(GLOBAL)`
- Refactored `pvpgn_v3_apply_flags()`:
  - Calls `pvpgn_v3_target_warnings(${target})` for the comprehensive warning set
  - Retains MSVC-only extras: `/Zc:__cplusplus /Zc:preprocessor /utf-8 /EHsc /we4834`
  - Retains GCC/Clang extras: `-Wno-unused-parameter -Werror=unused-result`
  - Calls `pvpgn_v3_target_werror(${target})` when `PVPGN_V3_WARNINGS_AS_ERRORS=ON`
- No duplicate warning flags — all comprehensive warnings now live in `v3_warnings.cmake`

## Design decisions
- Macros (not functions) used so that `target_compile_options` applies to the caller's scope target
- `pvpgn_v3_target_warnings` and `pvpgn_v3_target_werror` are separate so callers can apply warnings without `-Werror` (e.g. third-party headers, generated code)
- `pvpgn_v3_apply_flags` continues to be the single call site for all v3 library/test targets via `pvpgn_v3_add_library` and `pvpgn_v3_add_test`
