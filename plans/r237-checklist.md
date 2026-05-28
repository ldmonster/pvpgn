# R237 -- d2cs initconn classifier observation bridges

Status: **GREEN** (local Linux build:
`cmake --build build --target d2cs d2cs_legacy test_integration_legacy_d2cs_handle_init_bridge`)

## Scope

Fifth strangler-fig round in `src/d2cs/`. Adds the per-class helper
observation that fires AFTER `d2cs_handle_init_packet()` has decoded
the wire byte and dispatched to the matching helper:

| # | Helper                | Bridge                                      |
|---|-----------------------|---------------------------------------------|
| 1 | `on_d2gs_initconn(c)` | `pvpgn_v3_d2cs_on_d2gs_initconn_try(sd,addr)` |
| 2 | `on_d2cs_initconn(c)` | `pvpgn_v3_d2cs_on_d2cs_initconn_try(sd)`      |

Combined with R234(2) -- which already covers the pre-dispatch
`handle_init_packet_try` -- v3 now sees the full classification
pipeline: pre-decode (cclass), post-decode dispatch (d2gs / d2cs).

## Design notes

- Both helpers extend the EXISTING `handle_init_bridge.{hpp,cpp}` pair
  rather than creating new files. The module string
  `v3_d2cs_handle_init_bridge` is shared; messages disambiguate
  (`init packet observed` / `on d2gs initconn observed` /
  `on d2cs initconn observed`).
- Log level for both new symbols is `Info`, matching the
  `eventlog_level_info` lines they sit next to in legacy. The
  pre-dispatch symbol stays at `Trace` (it fires on every classified
  packet including future ones the bridge can't translate yet).
- The d2gs symbol receives the peer IPv4 (`d2cs_conn_get_addr()`,
  host-byte-order uint32). This lets v3 readers correlate to
  d2gslist entries without having to deref `t_connection`. The d2cs
  symbol intentionally takes ONLY the socket descriptor -- a client
  connection has no meaningful identity until login.

## Files added

- *(none)*

## Files modified

- `src/v3/integration/legacy_d2cs/include/integration/legacy_d2cs/handle_init_bridge.hpp`
  -- two new `extern "C"` declarations.
- `src/v3/integration/legacy_d2cs/src/handle_init_bridge.cpp` -- two
  new bridge bodies sharing the existing `render_int` / `render_uint`
  helpers.
- `src/d2cs/handle_init.cpp` -- two new forward decls appended to the
  existing `PVPGN_V3_D2CS_INTEGRATION` block; guarded calls at the
  top of `on_d2gs_initconn()` (before the `eventlog`) and at the top
  of `on_d2cs_initconn()` (before its `eventlog`).
- `tests/unit/integration/legacy_d2cs/handle_init_bridge_test.cpp`
  -- two new `TEST_CASE`s exercising the d2gs (sd + addr) and d2cs
  (sd only) symbols.

## Verify

```sh
cmake --build build --target \
    integration_legacy_d2cs \
    test_integration_legacy_d2cs_handle_init_bridge \
    d2cs_legacy d2cs -j$(nproc)

./build/tests/unit/integration/legacy_d2cs/test_integration_legacy_d2cs_handle_init_bridge \
    --reporter compact
```

Result: 5 TEST_CASEs, 30 assertions, all PASS. `d2cs_legacy` + `d2cs`
link clean.

## Lessons (memorialised)

- When a legacy module already owns a bridge `.hpp/.cpp` pair AND the
  new bridges share the same module-tag, extend the existing pair
  instead of creating a parallel one. This keeps the v3-side trace
  module compact and avoids a name like
  `v3_d2cs_on_d2gs_initconn_bridge` that would shatter readability
  for log aggregators.
- For a helper that runs AFTER a wire-decoded discriminator (here:
  cclass), the bridge call should sit at the very top of the helper
  body, BEFORE any side-effects (eventlog, state mutation). This
  ensures v3 observes the dispatch decision regardless of whether
  the helper later returns a non-zero error code.
- Two bridges fired on the same logical packet (handle_init_packet +
  on_d2gs_initconn) is intentional. v3's trace pipeline will dedup
  later via causality; observation should be redundant on legacy
  side so that strangling either half doesn't blind us to the other.
