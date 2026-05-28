# R241 -- d2cs net.cpp socket-helper observation bridges

Status: **GREEN** (local Linux build:
`cmake --build build --target d2cs d2cs_legacy test_integration_legacy_d2cs_net_bridge`)

## Scope

Ninth strangler-fig round in `src/d2cs/`. Opens up the socket-helper
module in `src/d2cs/net.cpp` -- the thin layer between psock_* and
the rest of d2cs -- by wrapping three entry points:

| # | Symbol                            | Bridge                                                     |
|---|-----------------------------------|------------------------------------------------------------|
| 1 | `net_socket(type)`                | `pvpgn_v3_d2cs_net_socket_try(type)`                       |
| 2 | `net_check_connected(sock)`       | `pvpgn_v3_d2cs_net_check_connected_try(sock)`              |
| 3 | `net_listen(ip, port, type)`      | `pvpgn_v3_d2cs_net_listen_try(ip, port, type)`             |

The `net_inet_addr`, `net_send_data`, and `net_recv_data` helpers in
the same file are deliberately NOT covered in this round -- their
syscall-tight loops happen on the hot path and observation would
require batching. Future rounds will treat them as a separate
concern.

## Design notes

- New bridge pair `net_bridge.{hpp,cpp}`; module
  `v3_d2cs_net_bridge`; messages `net socket observed`,
  `net check_connected observed`, `net listen observed`.
- Log levels chosen by call frequency:
  * `net_listen` -> `Info` (startup-only, single call per listener;
    survives default log filter).
  * `net_socket` -> `Debug` (per outbound connect attempt; medium
    frequency).
  * `net_check_connected` -> `Trace` (per readiness event on an
    in-flight outbound connect; can fire thousands of times per
    second under busy s2s peering).
- Every bridge fires BEFORE the syscall sequence so v3 sees the
  intent regardless of whether the underlying syscall later fails
  with `errno`. Observation-of-intent is the prior round's
  established pattern for destroy / lookup paths.
- `ip` and `port` are passed as host-byte-order uint32 (the legacy
  parameters before `htonl`/`htons`); v3 readers receive the raw
  values the caller intended.

## Files added

- `src/v3/integration/legacy_d2cs/include/integration/legacy_d2cs/net_bridge.hpp`
- `src/v3/integration/legacy_d2cs/src/net_bridge.cpp`
- `tests/unit/integration/legacy_d2cs/net_bridge_test.cpp`

## Files modified

- `src/v3/CMakeLists.txt` -- one new source wired into
  `integration_legacy_d2cs`.
- `src/d2cs/net.cpp` -- new forward-decl block (after setup_after.h)
  declaring all three bridge symbols under
  `PVPGN_V3_D2CS_INTEGRATION`; guarded calls at the top of
  `net_socket()`, `net_check_connected()`, and `net_listen()`,
  each after the local variable declarations and before the first
  `psock_*` call.
- `tests/unit/integration/legacy_d2cs/CMakeLists.txt` -- one new
  `pvpgn_v3_add_test(...)` entry.
- `Dockerfile.v3` -- new target added to both the explicit
  `cmake --build --target ...` list and the v3-test RUN chain.

## Verify

```sh
cmake --build build --target \
    integration_legacy_d2cs \
    test_integration_legacy_d2cs_net_bridge \
    d2cs_legacy d2cs -j$(nproc)

./build/tests/unit/integration/legacy_d2cs/test_integration_legacy_d2cs_net_bridge \
    --reporter compact
```

Result: 4 TEST_CASEs, 20 assertions, all PASS. `d2cs_legacy` + `d2cs`
link clean.

## Lessons (memorialised)

- For modules that mix hot-path I/O helpers (send/recv) with cold-
  path setup helpers (socket/listen/check_connected), bridge the
  cold-path helpers first. The hot-path helpers will require
  batching primitives that don't exist yet in the v3 trace
  pipeline.
- Mapping log level to syscall frequency keeps the trace stream
  navigable: startup events at `Info`, per-connection events at
  `Debug`, per-readiness-event observations at `Trace`. This
  matches the heuristic established in R234 (server_process =
  Info) + R236 (conn_destroy = Debug).
- Network-byte-order conversions belong inside the legacy syscall
  body, NOT in the bridge. The bridge captures the parameters the
  caller passed (host-byte-order); a v3 reader can apply htonl/
  htons itself if needed. This keeps the bridge ABI free of
  endianness assumptions and matches what the d2cs source code
  itself sees at its call sites.
