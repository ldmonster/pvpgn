# R233 -- Observation bridges for d2cs lifecycle (d2ladder + s2s + signals)

Status: **GREEN** (local Linux build:
`cmake --build build --target d2cs d2cs_legacy test_integration_legacy_d2cs_d2ladder_bridge test_integration_legacy_d2cs_s2s_bridge test_integration_legacy_d2cs_handle_signal_bridge`)

## Scope

First strangler-fig lifecycle round in the `src/d2cs/` tree, parallel
to R231/R232 for `src/d2dbs/`. Adds three observation-only bridge
pairs for the d2cs daemon's startup/shutdown surface, plus the new
`legacy_d2cs::bridge_logger` seam (the d2cs integration lib did not
have one yet -- `send_*` bridges talked to `core::default_logger()`
directly).

| # | Module             | Op(s)                                                       |
|---|--------------------|-------------------------------------------------------------|
| 1 | d2ladder.cpp       | `d2ladder_init()`, `d2ladder_destroy()`                     |
| 2 | s2s.cpp            | `s2s_init()` (outbound bnetd link bootstrap)                |
| 3 | handle_signal.cpp  | `handle_signal_init()` (POSIX), `handle_signal()` dispatch  |

## Design notes

- A new `pvpgn::integration::legacy_d2cs::bridge_logger()` seam is
  introduced (header + `.cpp` + `BridgeLoggerOverride` RAII guard),
  mirroring the existing `legacy_bnetd` and `legacy_d2dbs` versions.
  Single-binary mode embeds all three daemons in one process, so the
  three namespaces stay independent and can be overridden separately
  in tests.
- Bridge headers expose only C-linkage POD ABIs
  (`extern "C" int pvpgn_v3_d2cs_<op>_try(...) noexcept`). Headers do
  not include any legacy `src/d2cs/*` types, so the v3 integration lib
  stays out of `setup_before.h`/`setup_after.h` brackets.
- Symbol naming: `pvpgn_v3_d2cs_d2ladder_init_try` is intentionally
  prefixed with `d2cs_` because d2dbs already exports
  `pvpgn_v3_d2dbs_d2ladder_init_try` and single-binary mode would
  collide otherwise.
- Signal dispatch (`handle_signal()`) logs at `Trace` because it
  runs every server tick; init logs at `Debug`. `s2s_init` logs at
  `Info` since it fires once per process and represents the moment
  the realm-link transport comes up.

## Files added

- `src/v3/integration/legacy_d2cs/include/integration/legacy_d2cs/bridge_logger.hpp`
- `src/v3/integration/legacy_d2cs/src/bridge_logger.cpp`
- `src/v3/integration/legacy_d2cs/include/integration/legacy_d2cs/d2ladder_bridge.hpp`
- `src/v3/integration/legacy_d2cs/src/d2ladder_bridge.cpp`
- `src/v3/integration/legacy_d2cs/include/integration/legacy_d2cs/s2s_bridge.hpp`
- `src/v3/integration/legacy_d2cs/src/s2s_bridge.cpp`
- `src/v3/integration/legacy_d2cs/include/integration/legacy_d2cs/handle_signal_bridge.hpp`
- `src/v3/integration/legacy_d2cs/src/handle_signal_bridge.cpp`
- `tests/unit/integration/legacy_d2cs/d2ladder_bridge_test.cpp`
- `tests/unit/integration/legacy_d2cs/s2s_bridge_test.cpp`
- `tests/unit/integration/legacy_d2cs/handle_signal_bridge_test.cpp`

## Files modified

- `src/v3/CMakeLists.txt` -- 4 new sources wired into
  `integration_legacy_d2cs` (parent lib, not the `_linked` variant).
- `src/d2cs/d2ladder.cpp` -- forward decls after `setup_after.h`,
  guarded calls in `d2ladder_init()` and `d2ladder_destroy()`.
- `src/d2cs/s2s.cpp` -- forward decl after `setup_after.h`, guarded
  call in `s2s_init()`.
- `src/d2cs/handle_signal.cpp` -- forward decls after
  `setup_after.h`, guarded calls in `handle_signal()` (all platforms,
  top of body) and `handle_signal_init()` (POSIX only; WIN32 uses a
  different set of console-control wrappers and has no
  `handle_signal_init()` to instrument).
- `tests/unit/integration/legacy_d2cs/CMakeLists.txt` -- registers
  three new `pvpgn_v3_add_test(...)` entries.
- `Dockerfile.v3` -- three new targets added to both the explicit
  `cmake --build --target ...` list (v3-build stage) and to the
  v3-test RUN chain with `--reporter compact`.

## Verify

```sh
cmake -S . -B build
cmake --build build --target \
    integration_legacy_d2cs \
    test_integration_legacy_d2cs_d2ladder_bridge \
    test_integration_legacy_d2cs_s2s_bridge \
    test_integration_legacy_d2cs_handle_signal_bridge \
    d2cs_legacy d2cs -j$(nproc)

./build/tests/unit/integration/legacy_d2cs/test_integration_legacy_d2cs_d2ladder_bridge      --reporter compact
./build/tests/unit/integration/legacy_d2cs/test_integration_legacy_d2cs_s2s_bridge           --reporter compact
./build/tests/unit/integration/legacy_d2cs/test_integration_legacy_d2cs_handle_signal_bridge --reporter compact
```

Result: 8 TEST_CASEs, 37 assertions, all PASS. `d2cs_legacy` + `d2cs`
link clean.

## Lessons (memorialised)

- When a daemon's integration lib has no `bridge_logger` seam yet,
  introduce it FIRST (header + `.cpp` + `BridgeLoggerOverride`) and
  only then add the lifecycle bridges that depend on it. Otherwise
  every new bridge has to be retrofitted later.
- Keep each daemon's `bridge_logger` in its OWN namespace
  (`legacy_bnetd::`, `legacy_d2dbs::`, `legacy_d2cs::`). Single-binary
  mode embeds all three; sharing a seam would let one daemon's test
  override silently capture another daemon's records.
- d2cs and d2dbs both ship a `d2ladder_init()` symbol. v3 bridge
  symbols MUST encode the daemon name (`pvpgn_v3_d2cs_*` vs
  `pvpgn_v3_d2dbs_*`) to avoid link-time collisions when the static
  archives are eventually combined for single-binary mode.
- `extern "C"` forward decls in legacy `.cpp` files MUST appear AFTER
  `#include "common/setup_after.h"`. The setup headers reshape
  preprocessor symbols that affect linkage on Windows.
