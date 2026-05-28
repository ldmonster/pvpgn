# R214 -- [[nodiscard]] audit and enforcement

**Date:** 2026-05-27
**Round:** 214
**Phase:** 1 (Foundations)
**Plan ref:** [plans/01-modern-cpp-baseline.md](../plans/01-modern-cpp-baseline.md) §3
**Status:** GREEN ✓

## Audit summary

- `core::Result<T,E>` and the `Result<void,E>` specialisation (which
  `Status<T>` aliases) are **already `[[nodiscard]]` at the class
  level** -- so every function in the v3 tree that returns a Result
  or Status is implicitly nodiscard. The risk was only that the
  warning was not promoted to an error.
- Only four sites in `src/v3/` explicitly discard a Status with
  `(void)`, all in `protocol/telnet/admin_fsm.cpp` and intentional
  (fire-and-forget logging in shutdown / error paths).
- No `core::Bytes` aliases existed (verified in R212). No new wrapper
  types need `[[nodiscard]]`.

## Changes

- [x] `src/v3/core/include/core/error.hpp`:
  - `[[nodiscard]]` on `Error::code()`, `Error::message()`,
    `Error::is_ok()`. Calling these for their side effects is always
    a bug.
  - `[[nodiscard]]` on `make_error(...)` factory.

- [x] `src/v3/core/include/core/result.hpp`:
  - `[[nodiscard]]` on `Result::has_value()`, `Result::operator bool()`,
    `Result::value_or(...)` for both the primary template and the
    `Result<void,E>` specialisation.
  - `[[nodiscard]]` on `ok()` factory.
  - Added a doc block ("R214 nodiscard convention") that codifies
    the project rule and prescribes `(void)expr;` for the rare
    intentional discard.

- [x] `cmake/v3.cmake`:
  - GCC/Clang: added `-Werror=unused-result` to `pvpgn_v3_apply_flags`.
    A discarded `[[nodiscard]]` return value is now a **hard build
    error**, not just a warning.
  - MSVC: added `/we4834` (C4834 is the equivalent warning on MSVC).

## Build verification

```powershell
docker build -f Dockerfile.v3 --target v3-test -t pvpgn-v3-test:r214 .
```

GREEN. Image tagged. **Every v3 unit, integration, and bridge target
compiled cleanly under `-Werror=unused-result`** -- meaning zero
existing call sites silently drop a `Result` or `Status`. The four
intentional discards in `protocol/telnet/admin_fsm.cpp` were
correctly cast to `(void)` already.

## Why this matters

A silently-discarded `Result` is the single most common failure mode
in Result/Either-style error handling: the compiler used to merely
warn (and we did not even build with `-Werror` selectively), so a
human reader was the last line of defence. From R214 onwards a
forgotten `auto r = io.write(...);` cannot reach `main`.

## Out of scope (recorded as follow-ups)

- Adding `[[nodiscard]]` to value-object accessors
  (`BNHash::bytes()`, `ClientTag::bytes()`, `IpAddress::*`,
  `Account::*`, etc.). Low ROI -- ignoring `bytes()` for its side
  effect is not a realistic bug.
- Promoting full `-Werror` on v3 (covered by `PVPGN_V3_WARNINGS_AS_ERRORS`
  which the canonical Docker build does not enable yet); separate
  hardening round.
- Auditing legacy `bnetd_legacy` / `d2cs_legacy` / `d2dbs_legacy`
  trees -- they don't go through `pvpgn_v3_apply_flags` and are
  scheduled for retirement (plan 14).

## Files touched

### Modified
- `src/v3/core/include/core/error.hpp`
- `src/v3/core/include/core/result.hpp`
- `cmake/v3.cmake`

### Created
- `plans/r214-checklist.md` -- this file.
