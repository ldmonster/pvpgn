# R211 -- std::format Migration (cleanup)

**Date:** 2026-05-27
**Round:** 211
**Phase:** 1 (Foundations)
**Plan ref:** [plans/01-modern-cpp-baseline.md](../plans/01-modern-cpp-baseline.md) §2
**Status:** GREEN ✓

## Objective

The original plan-01 §2 entry assumed `core/format.hpp` was an fmt facade
that needed to be migrated to `std::format`. **Audit found this was
already done**: `core/format.hpp` calls `std::format` directly and only
fell back to a no-op pass-through when `__cpp_lib_format` was absent.

With C++20 mandatory after R210, R211 reduces to two cleanups:

1. Drop the dead `PVPGN_V3_HAS_STD_FORMAT == 0` fallback in
   `core/format.hpp`. That fallback would silently emit unformatted
   strings on older stdlibs -- a debugging footgun. Replaced with a
   hard `#error` for libstdc++ < 13 / MSVC < 19.29.
2. Replace the one remaining `fmt::` usage in the relocated legacy
   `handle_bnet_link.cpp` (`fmt::memory_buffer` + `fmt::format_to` +
   `fmt::to_string` -> `std::string` + `std::format_to`).

## Changes applied

- [x] `src/v3/core/include/core/format.hpp`:
  - Removed `PVPGN_V3_HAS_STD_FORMAT` macro and its dual code paths.
  - Replaced with unconditional `#include <format>` + `#error` guard
    on `__cpp_lib_format < 201907L`.
- [x] `src/v3/integration/legacy_bnetd/src/handle_bnet_link.cpp`:
  - `fmt::memory_buffer serverinfo;` -> `std::string serverinfo;`
  - `fmt::format_to(std::back_inserter(serverinfo), ...)` ->
    `std::format_to(std::back_inserter(serverinfo), ...)` (signature
    is identical; std::back_inserter on std::string is the canonical
    target).
  - `packet_append_string(rpacket, fmt::to_string(serverinfo).c_str())`
    -> `packet_append_string(rpacket, serverinfo.c_str())`.
  - Added `<format>` and `<string>` to the explicit includes.

## Scope clarification

- `fmt::` is still used heavily by the **legacy** `src/bnetd/`,
  `src/d2cs/`, `src/d2dbs/` trees and by `bnetd_legacy/i18n.h`. Those
  are scheduled for retirement (plan 14). R211 is the v3-tree cleanup.
- After R211 there are **zero** `fmt::` references in `src/v3/**`.

## Build verification

```powershell
docker build -f Dockerfile.v3 --target v3-test -t pvpgn-v3-test:r211 .
```

GREEN. Image tagged. `test_core_headers_selfcontained` re-verified that
`core/format.hpp` is still self-contained after dropping the fallback,
and the legacy-bnetd integration shim compiles with `std::format_to`.

## Files touched

### Modified
- `src/v3/core/include/core/format.hpp`
- `src/v3/integration/legacy_bnetd/src/handle_bnet_link.cpp`

### Created
- `plans/r211-checklist.md` -- this file.
