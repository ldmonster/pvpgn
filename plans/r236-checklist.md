# R236 -- d2cs queue + d2gs-list lifecycle observation bridges

Status: **GREEN** (local Linux build:
`cmake --build build --target d2cs d2cs_legacy test_integration_legacy_d2cs_gamequeue_bridge test_integration_legacy_d2cs_serverqueue_bridge test_integration_legacy_d2cs_d2gs_bridge`)

## Scope

Fourth strangler-fig round in `src/d2cs/`. Wraps the create/destroy
(and one reload) entry points of the three intra-process queues /
catalogues that the d2cs daemon owns.

| # | Module             | Op(s)                                                      |
|---|--------------------|------------------------------------------------------------|
| 1 | gamequeue.cpp      | `gqlist_create()`, `gqlist_destroy()`                      |
| 2 | serverqueue.cpp    | `sqlist_create()`, `sqlist_destroy()`                      |
| 3 | d2gs.cpp           | `d2gslist_create()`, `d2gslist_destroy()`, `d2gslist_reload(gslist)` |

After R233 + R234 + R235 + R236 the `src/d2cs/` instrumentation now
covers: bridge_logger seam, all lifecycle entry points (server loop,
signals, ladder, s2s, per-conn destroy, init packet, all three queue
catalogues), and all three packet dispatchers (client / d2gs / bnetd
s2s).

## Design notes

- `d2gslist_reload` takes a `const char* gslist` -- the bridge logs
  the pointer's null-state and, when non-null, captures the string
  via `std::string_view` directly. The caller (the prefs subsystem or
  the SIGHUP handler) owns the buffer for the entire duration of the
  legacy `d2gslist_reload` body, which strictly outlives the bridge
  call, so the `string_view` cannot dangle.
- The bridge intentionally does NOT parse the gslist -- the legacy
  body still owns `addrlist_create()`. Future v3 work that wants to
  inspect parsed addrs should add a SECOND bridge call site after
  parse completes (so v3 sees only validated input).
- Log level: `Debug` for every R236 bridge. None of these fire on
  hot paths.

## Files added

- `src/v3/integration/legacy_d2cs/include/integration/legacy_d2cs/gamequeue_bridge.hpp`
- `src/v3/integration/legacy_d2cs/src/gamequeue_bridge.cpp`
- `src/v3/integration/legacy_d2cs/include/integration/legacy_d2cs/serverqueue_bridge.hpp`
- `src/v3/integration/legacy_d2cs/src/serverqueue_bridge.cpp`
- `src/v3/integration/legacy_d2cs/include/integration/legacy_d2cs/d2gs_bridge.hpp`
- `src/v3/integration/legacy_d2cs/src/d2gs_bridge.cpp`
- `tests/unit/integration/legacy_d2cs/gamequeue_bridge_test.cpp`
- `tests/unit/integration/legacy_d2cs/serverqueue_bridge_test.cpp`
- `tests/unit/integration/legacy_d2cs/d2gs_bridge_test.cpp`

## Files modified

- `src/v3/CMakeLists.txt` -- 3 new sources wired into
  `integration_legacy_d2cs`.
- `src/d2cs/gamequeue.cpp` -- forward decls + guarded calls in
  `gqlist_create()` / `gqlist_destroy()`.
- `src/d2cs/serverqueue.cpp` -- forward decls + guarded calls in
  `sqlist_create()` / `sqlist_destroy()`.
- `src/d2cs/d2gs.cpp` -- new forward decls appended to the existing
  `PVPGN_V3_D2CS_INTEGRATION` block; guarded calls in
  `d2gslist_create()`, `d2gslist_reload()` (after locals declared,
  before the `d2gslist_head` null-check), `d2gslist_destroy()`.
- `tests/unit/integration/legacy_d2cs/CMakeLists.txt` -- registers
  three new `pvpgn_v3_add_test(...)` entries.
- `Dockerfile.v3` -- three new targets added to both the explicit
  `cmake --build --target ...` list and the v3-test RUN chain.

## Verify

```sh
cmake -S . -B build
cmake --build build --target \
    integration_legacy_d2cs \
    test_integration_legacy_d2cs_gamequeue_bridge \
    test_integration_legacy_d2cs_serverqueue_bridge \
    test_integration_legacy_d2cs_d2gs_bridge \
    d2cs_legacy d2cs -j$(nproc)

for t in gamequeue_bridge serverqueue_bridge d2gs_bridge; do
    ./build/tests/unit/integration/legacy_d2cs/test_integration_legacy_d2cs_$t --reporter compact
done
```

Result: 11 TEST_CASEs, 46 assertions, all PASS. `d2cs_legacy` + `d2cs`
link clean.

## Lessons (memorialised)

- A C-string argument from a legacy entry point can be safely
  forwarded as `std::string_view` to a bridge IF: (a) the bridge is
  observation-only and synchronous, (b) the legacy caller's body
  outlives the bridge call, and (c) the bridge handles a null
  pointer explicitly (here: rendered as `"<null>"`).
- For lifecycle bridges, place the guarded call BEFORE any code that
  could short-circuit and return. For `d2gslist_reload` that meant
  placing the call BEFORE `if (!d2gslist_head) return -1;` so the
  bridge fires on every attempt, not just on successful reloads.
  This matches v3's "trace every legacy entry, observe legacy
  outcome later" model.
- Multiple lifecycle ops in one legacy module map to multiple
  symbols in ONE bridge `.hpp/.cpp` pair (one pair per legacy
  module). The d2gs bridge ships three symbols; the gamequeue and
  serverqueue bridges ship two each.
