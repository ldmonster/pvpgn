# R212 -- Bytes / std::span canonical port-boundary type

**Date:** 2026-05-27
**Round:** 212
**Phase:** 1 (Foundations)
**Plan ref:** [plans/01-modern-cpp-baseline.md](../plans/01-modern-cpp-baseline.md) §1 row "custom byte buffers"
**Status:** GREEN ✓

## Audit

The plan-01 checkbox reads *"R212: migrate `core::Bytes` aliases to
`std::span<const std::byte>`"*. The audit shows:

- **No `core::Bytes` alias exists** anywhere in `src/v3/`. The only
  `Bytes` name is `domain::shared::BNHash::Bytes`, which is a
  *nested* alias for a fixed-size 20-byte digest array — not a
  byte-buffer concept.
- `core::ByteView` (=`std::span<const std::byte>`) and `core::ByteSpan`
  (=`std::span<std::byte>`) are already defined in
  [src/v3/core/include/core/bytes.hpp](../src/v3/core/include/core/bytes.hpp).
- The canonical style is already used in many places:
  `protocol/wol/wol_session_context::send_bytes(std::span<const std::byte>)`,
  `protocol/wolgameres/wol_fsm::on_bytes(std::span<const std::byte>)`,
  the endian helpers in `core/endian.hpp`, and most codecs.
- Remaining `std::span<const uint8_t>` uses cluster into three
  buckets:
  1. **Integration session `feed()` boundaries** (wol, telnet, irc) -
     true port boundaries; should be canonical. (Scope of R212.)
  2. **Owning storage** in DTOs (e.g. `application/realm/save_data`,
     `application/anongame_infoply/CompressedPayload`) - byte-as-integer
     storage that round-trips through SQL/Lua/legacy APIs; valid use.
  3. **Internal byte-as-integer helpers** (e.g. `domain/realm/dupe_checker`
     hashing/parsing). Out of scope; not a layer boundary.

## Scope

R212 = establish `core::ByteView` as the canonical type at the
**integration session entry points** and document the rule.

## Changes

- [x] `src/v3/core/include/core/bytes.hpp` -- added a doc block above
  the `ByteSpan`/`ByteView` aliases that names them as the canonical
  port-boundary types and clarifies when `std::vector<uint8_t>` and
  `std::span<const uint8_t>` remain valid (internal helpers, owning
  storage that crosses into SQL/Lua/legacy).

- [x] `src/v3/integration/wol/include/integration/wol/wol_session_factory.hpp`
  + `src/v3/integration/wol/src/wol_session.cpp` -- `WolSession::feed`
  now takes `core::ByteView`. The internal accumulation buffer remains
  `std::vector<uint8_t>` (storage), with a single `reinterpret_cast`
  at the boundary.

- [x] `src/v3/integration/telnet/include/integration/telnet/telnet_session_factory.hpp`
  + `src/v3/integration/telnet/src/telnet_session.cpp` --
  `TelnetSession::feed` now takes `core::ByteView`. Loop body
  decodes each `std::byte` to `uint8_t` for the existing IAC / ASCII
  range checks.

- [x] `src/v3/integration/irc/include/integration/irc/irc_session_factory.hpp`
  + `src/v3/integration/irc/src/irc_session.cpp` -- same pattern as
  telnet.

- [x] `tests/unit/integration/{wol,telnet,irc}/*_session_test.cpp` --
  updated all `feed(std::span<const uint8_t>(vec))` call sites to
  `feed(core::as_byte_view(vec.data(), vec.size()))` and added
  `#include "core/bytes.hpp"`. (Note: these test files use
  `<gtest/gtest.h>` while the v3 project uses Catch2, and they are
  not in the `Dockerfile.v3` v3-build target list -- so they don't
  actually compile in CI. The signature update keeps them in sync
  for whoever fixes the framework mismatch later.)

## Out of scope (recorded as follow-ups)

- `domain::realm::DupeChecker` interface and helpers
  (`std::span<const uint8_t>`) -- internal hashing helpers, not a
  layer boundary. Touch when DupeChecker grows a real port.
- `application::realm::{Save,Load}Character::save_data`,
  `domain::d2dbs::*::data`, `domain::identity::AccountSnapshot::
  command_groups`, `application::anongame_infoply::CompressedPayload::*`
  -- owning storage of byte-blobs that round-trip through SQL / Lua /
  legacy `char*` APIs. The plan-01 table prescribes
  `std::vector<std::byte>` long-term; defer to the persistence
  refactor (plan 07) which will redesign those DTOs anyway.
- `integration::legacy_*` shims -- frozen until plan 14 retirement.
- Test framework mismatch in `tests/unit/integration/{wol,telnet,irc}/`
  (gtest vs Catch2). Separate cleanup round.

## Build verification

```powershell
docker build -f Dockerfile.v3 --target v3-test -t pvpgn-v3-test:r212 .
```

GREEN. Image tagged. No new compile errors; all enabled v3 unit
tests pass.

## Files touched

### Modified
- `src/v3/core/include/core/bytes.hpp`
- `src/v3/integration/wol/include/integration/wol/wol_session_factory.hpp`
- `src/v3/integration/wol/src/wol_session.cpp`
- `src/v3/integration/telnet/include/integration/telnet/telnet_session_factory.hpp`
- `src/v3/integration/telnet/src/telnet_session.cpp`
- `src/v3/integration/irc/include/integration/irc/irc_session_factory.hpp`
- `src/v3/integration/irc/src/irc_session.cpp`
- `tests/unit/integration/wol/wol_session_test.cpp`
- `tests/unit/integration/telnet/telnet_session_test.cpp`
- `tests/unit/integration/irc/irc_session_test.cpp`

### Created
- `plans/r212-checklist.md` -- this file.
