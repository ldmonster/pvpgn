# R235 -- d2cs packet-dispatcher observation bridges

Status: **GREEN** (local Linux build:
`cmake --build build --target d2cs d2cs_legacy test_integration_legacy_d2cs_handle_d2cs_packet_bridge test_integration_legacy_d2cs_handle_d2gs_packet_bridge test_integration_legacy_d2cs_handle_bnetd_packet_bridge`)

## Scope

Third strangler-fig round in `src/d2cs/`. After R234 covered the
once-per-process / once-per-connection lifecycle, R235 instruments
the per-packet dispatcher entry points that fan out into the per-type
handler tables (one entry-point per connection class):

| # | Module              | Op(s)                                                  |
|---|---------------------|--------------------------------------------------------|
| 1 | handle_d2cs.cpp     | `d2cs_handle_d2cs_packet(c, packet)` (client class)    |
| 2 | handle_d2gs.cpp     | `handle_d2gs_packet(c, packet)`     (d2gs class)       |
| 3 | handle_bnetd.cpp    | `handle_bnetd_packet(c, packet)`    (bnetd s2s class)  |

Each dispatcher pre-extracts `packet_get_type` + `packet_get_size`
before delegating to `conn_process_packet(...)`; the bridge fires
with those scalars + the connection socket descriptor.

## Design notes

- All three bridges share an identical ABI:
  `pvpgn_v3_d2cs_handle_<role>_packet_try(int sd, unsigned int packet_type, unsigned int packet_size)`,
  returning 0 (legacy fall-through). They are still split into three
  modules so each call-site lives in its own translation unit and can
  be wired into the legacy `.cpp` independently; this also keeps the
  audit trail in `plans/` 1:1 with legacy modules.
- Log level: `Trace`. These fire on every inbound packet on a live
  d2cs connection -- production builds will gate them behind
  `set_level()` so the cost is one extern call + early-out.
- Scalars are rendered via `std::to_chars` into local
  `std::array<char, 20>` buffers (same template as R232/R234).
- The forward decls live inside the existing
  `#ifdef PVPGN_V3_D2CS_INTEGRATION` block at the top of each
  legacy `.cpp` (next to the older `send_*` strangler-fig hooks).

## Files added

- `src/v3/integration/legacy_d2cs/include/integration/legacy_d2cs/handle_d2cs_packet_bridge.hpp`
- `src/v3/integration/legacy_d2cs/src/handle_d2cs_packet_bridge.cpp`
- `src/v3/integration/legacy_d2cs/include/integration/legacy_d2cs/handle_d2gs_packet_bridge.hpp`
- `src/v3/integration/legacy_d2cs/src/handle_d2gs_packet_bridge.cpp`
- `src/v3/integration/legacy_d2cs/include/integration/legacy_d2cs/handle_bnetd_packet_bridge.hpp`
- `src/v3/integration/legacy_d2cs/src/handle_bnetd_packet_bridge.cpp`
- `tests/unit/integration/legacy_d2cs/handle_d2cs_packet_bridge_test.cpp`
- `tests/unit/integration/legacy_d2cs/handle_d2gs_packet_bridge_test.cpp`
- `tests/unit/integration/legacy_d2cs/handle_bnetd_packet_bridge_test.cpp`

## Files modified

- `src/v3/CMakeLists.txt` -- 3 new sources wired into
  `integration_legacy_d2cs`.
- `src/d2cs/handle_d2cs.cpp` -- new forward decl appended to the
  existing `PVPGN_V3_D2CS_INTEGRATION` block; guarded call at the top
  of `d2cs_handle_d2cs_packet()` before `conn_process_packet`.
- `src/d2cs/handle_d2gs.cpp` -- same pattern; call at top of
  `handle_d2gs_packet()`.
- `src/d2cs/handle_bnetd.cpp` -- same pattern; call at top of
  `handle_bnetd_packet()`.
- `tests/unit/integration/legacy_d2cs/CMakeLists.txt` -- registers
  three new `pvpgn_v3_add_test(...)` entries.
- `Dockerfile.v3` -- three new targets added to both the explicit
  `cmake --build --target ...` list and the v3-test RUN chain.

## Verify

```sh
cmake -S . -B build
cmake --build build --target \
    integration_legacy_d2cs \
    test_integration_legacy_d2cs_handle_d2cs_packet_bridge \
    test_integration_legacy_d2cs_handle_d2gs_packet_bridge \
    test_integration_legacy_d2cs_handle_bnetd_packet_bridge \
    d2cs_legacy d2cs -j$(nproc)

for t in handle_d2cs_packet_bridge handle_d2gs_packet_bridge handle_bnetd_packet_bridge; do
    ./build/tests/unit/integration/legacy_d2cs/test_integration_legacy_d2cs_$t --reporter compact
done
```

Result: 6 TEST_CASEs, 32 assertions, all PASS. `d2cs_legacy` + `d2cs`
link clean.

## Lessons (memorialised)

- When a legacy `.cpp` already has a `PVPGN_V3_D2CS_INTEGRATION`
  forward-decl block, append to that existing block rather than
  opening a parallel one -- the macro guard stays cohesive and the
  diff is smaller.
- Three siblings with identical signatures still get split into three
  files. The bridges are observation only TODAY but each is the seam
  where the v3 dispatcher will eventually attach; keeping them split
  means swapping any one of them to a real (non-zero) implementation
  later is local.
- Pre-extract wire scalars (`packet_get_type/size`) on the legacy
  side. The bridge never touches `t_packet` so the integration lib's
  public ABI stays legacy-header-free.
