# R242 -- d2dbs packet dispatcher observation bridges

Status: **GREEN** (local Linux build:
`cmake --build build --target d2dbs d2dbs_legacy test_integration_legacy_d2dbs_dbspacket_bridge`)

## Scope

Pivot back to d2dbs after the long d2cs run (R233..R241). Wraps the
three top-level packet processing entry points in
`src/d2dbs/dbspacket.cpp`:

| # | Symbol                       | Bridge                                                              |
|---|------------------------------|---------------------------------------------------------------------|
| 1 | `dbs_packet_handle(conn)`    | `pvpgn_v3_d2dbs_packet_handle_try(sd, stats, type)`                 |
| 2 | `dbs_check_timeout()`        | `pvpgn_v3_d2dbs_check_timeout_try()`                                |
| 3 | `dbs_keepalive()`            | `pvpgn_v3_d2dbs_keepalive_try()`                                    |

Together with R231 / R232 the d2dbs daemon now has observation
coverage for: bridge_logger seam, charlock lifecycle, d2ladder,
dbsdupecheck, dbserver, handle_signal, and now the per-tick packet
processing trio.

## Design notes

- New bridge pair `dbspacket_bridge.{hpp,cpp}` in
  `src/v3/integration/legacy_d2dbs/`. Module
  `v3_d2dbs_dbspacket_bridge`; messages disambiguate (`packet
  handle observed`, `check_timeout observed`, `keepalive observed`).
- `packet_handle` fires at the top of the function so v3 sees every
  attempt (including the `stats == 0` initial-handshake branch and
  the per-message drain branch). The bridge captures the connection
  `(sd, stats, type)` triple -- enough for v3 to correlate to the
  upstream `t_d2dbs_connection` without ever dereferencing it.
- `check_timeout` and `keepalive` are per-tick periodic events
  (called from the d2dbs main loop). Log level is `Debug` -- they
  fire on every loop iteration and would saturate Info-level logs.
- `packet_handle` is `Trace` -- it fires on every packet which can
  be thousands of times per second under load.
- The d2dbs daemon already had a partial `PVPGN_V3_D2DBS_INTEGRATION`
  block at the top of dbspacket.cpp (the R225-era
  `pvpgn_v3_d2dbs_send_echorequest` decl); R242 appends the three
  new decls into that existing block, mirroring the convention
  established for d2cs in R235 (extend, don't parallel).

## Files added

- `src/v3/integration/legacy_d2dbs/include/integration/legacy_d2dbs/dbspacket_bridge.hpp`
- `src/v3/integration/legacy_d2dbs/src/dbspacket_bridge.cpp`
- `tests/unit/integration/legacy_d2dbs/dbspacket_bridge_test.cpp`

## Files modified

- `src/v3/CMakeLists.txt` -- one new source wired into
  `integration_legacy_d2dbs`.
- `src/d2dbs/dbspacket.cpp` -- three new forward decls appended to
  the existing `PVPGN_V3_D2DBS_INTEGRATION` block; guarded calls
  at the top of `dbs_packet_handle()` (after locals declared,
  before the stats-state switch), `dbs_check_timeout()` (before
  the timeout fetch), and `dbs_keepalive()` (before the
  writelen/now initialisation).
- `tests/unit/integration/legacy_d2dbs/CMakeLists.txt` -- one new
  `pvpgn_v3_add_test(...)` entry.
- `Dockerfile.v3` -- new target added to both the explicit
  `cmake --build --target ...` list and the v3-test RUN chain.

## Verify

```sh
cmake --build build --target \
    integration_legacy_d2dbs \
    test_integration_legacy_d2dbs_dbspacket_bridge \
    d2dbs_legacy d2dbs -j$(nproc)

./build/tests/unit/integration/legacy_d2dbs/test_integration_legacy_d2dbs_dbspacket_bridge \
    --reporter compact
```

Result: 4 TEST_CASEs, 20 assertions, all PASS. `d2dbs_legacy` + `d2dbs`
link clean.

## Lessons (memorialised)

- The d2cs and d2dbs bridge headers use different namespaces
  (`pvpgn::integration::legacy_d2cs` vs
  `pvpgn::integration::legacy_d2dbs`) AND different C-symbol
  prefixes (`pvpgn_v3_d2cs_*` vs `pvpgn_v3_d2dbs_*`). Reusing the
  d2cs pattern verbatim in d2dbs would have caused ODR clashes at
  link time in the single-binary build. The dual-prefix discipline
  scales.
- When a legacy file already has a `PVPGN_V3_<DAEMON>_INTEGRATION`
  block, append new forward decls into it rather than open a
  second `#ifdef` block. By R242 the dbspacket.cpp block holds 4
  declarations spanning two strangler-fig rounds; the same file
  in d2cs (handle_d2cs.cpp, handle_d2gs.cpp, d2gs.cpp) used the
  same convention from R235 onward.
- Pivoting between daemons mid-run is cheap because each bridge
  pair lives in an isolated `_bridge.{hpp,cpp}` file with its own
  forward-decl block on the legacy side. The cost is just keeping
  the daemon-specific helper `bridge_log_kv` callable through the
  correct namespace alias (`plc` for d2cs, `pld` for d2dbs).
