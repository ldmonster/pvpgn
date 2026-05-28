# R230 -- Lifecycle / subprocess strangler shells for orphaned bnetd modules

Status: **GREEN** (`pvpgn-v3-test:r230`)

## Scope

Add observation-only strangler-fig bridges (the
`pvpgn_v3_<op>_try` + guarded `#ifdef PVPGN_V3_BNETD_INTEGRATION`
call site pattern -- see `/memories`) for four legacy `src/bnetd/`
modules that today are still invoked exclusively through legacy
bodies, with no v3 dispatch point. Each new bridge:

1. Adds a header in
   `src/v3/integration/legacy_bnetd/include/integration/legacy_bnetd/`.
2. Adds a `.cpp` returning 0 (legacy fall-through) and emitting a
   structured kv log via `bridge_log_kv`.
3. Wires the `.cpp` into `integration_legacy_bnetd` via
   `src/v3/CMakeLists.txt` (the parent lib hosting all bridge `.cpp`
   files; do NOT use `integration_legacy_bnetd_linked`).
4. Adds an `#ifdef PVPGN_V3_BNETD_INTEGRATION` guarded call from the
   legacy site with a forward declaration.
5. Adds a Catch2 unit test under
   `tests/unit/integration/legacy_bnetd/`.
6. Adds the test binary to the explicit `--target` list and
   `--reporter compact` block in `Dockerfile.v3` (v3-build /
   v3-test stages).

Same shape as the R209-vintage `anongame_infos_bridge` family.

## Targets

| # | Module             | Op(s)                                            | Status |
|---|--------------------|--------------------------------------------------|--------|
| 1 | runprog.cpp        | `runprog_open`, `runprog_close`                  | [x]    |
| 2 | news.cpp           | `news_load`, `news_unload`                       | [x]    |
| 3 | userlog.cpp        | `userlog_init`, `userlog_append`                 | [x]    |
| 4 | versioncheck.cpp   | `load_versioncheck_conf`, `unload_versioncheck_conf` | [x]|

Out of scope for R230 (deferred): `topic.cpp` and `watch.cpp` -- both
are C++ class-based (`class_topic`, `WatchComponent`) with no
free-function `*_load/_unload` entry points; instrumenting them
cleanly requires per-method hooks and is its own round.

## Files modified

- [src/v3/integration/legacy_bnetd/include/integration/legacy_bnetd/runprog_bridge.hpp](src/v3/integration/legacy_bnetd/include/integration/legacy_bnetd/runprog_bridge.hpp) -- NEW
- [src/v3/integration/legacy_bnetd/src/runprog_bridge.cpp](src/v3/integration/legacy_bnetd/src/runprog_bridge.cpp) -- NEW
- [src/v3/integration/legacy_bnetd/include/integration/legacy_bnetd/news_bridge.hpp](src/v3/integration/legacy_bnetd/include/integration/legacy_bnetd/news_bridge.hpp) -- NEW
- [src/v3/integration/legacy_bnetd/src/news_bridge.cpp](src/v3/integration/legacy_bnetd/src/news_bridge.cpp) -- NEW
- [src/v3/integration/legacy_bnetd/include/integration/legacy_bnetd/userlog_bridge.hpp](src/v3/integration/legacy_bnetd/include/integration/legacy_bnetd/userlog_bridge.hpp) -- NEW
- [src/v3/integration/legacy_bnetd/src/userlog_bridge.cpp](src/v3/integration/legacy_bnetd/src/userlog_bridge.cpp) -- NEW
- [src/v3/integration/legacy_bnetd/include/integration/legacy_bnetd/versioncheck_bridge.hpp](src/v3/integration/legacy_bnetd/include/integration/legacy_bnetd/versioncheck_bridge.hpp) -- NEW
- [src/v3/integration/legacy_bnetd/src/versioncheck_bridge.cpp](src/v3/integration/legacy_bnetd/src/versioncheck_bridge.cpp) -- NEW
- [src/v3/CMakeLists.txt](src/v3/CMakeLists.txt) -- 4 bridges added to `integration_legacy_bnetd` source list.
- [src/bnetd/runprog.cpp](src/bnetd/runprog.cpp) -- 2 guarded call sites + fwd decls.
- [src/bnetd/news.cpp](src/bnetd/news.cpp) -- 2 guarded call sites + fwd decls.
- [src/bnetd/userlog.cpp](src/bnetd/userlog.cpp) -- 2 guarded call sites + fwd decls.
- [src/bnetd/versioncheck.cpp](src/bnetd/versioncheck.cpp) -- 2 guarded call sites + fwd decls.
- [tests/unit/integration/legacy_bnetd/runprog_bridge_test.cpp](tests/unit/integration/legacy_bnetd/runprog_bridge_test.cpp) -- NEW (3 TEST_CASEs, 13 assertions).
- [tests/unit/integration/legacy_bnetd/news_bridge_test.cpp](tests/unit/integration/legacy_bnetd/news_bridge_test.cpp) -- NEW (3 TEST_CASEs, 13 assertions).
- [tests/unit/integration/legacy_bnetd/userlog_bridge_test.cpp](tests/unit/integration/legacy_bnetd/userlog_bridge_test.cpp) -- NEW (3 TEST_CASEs, 15 assertions).
- [tests/unit/integration/legacy_bnetd/versioncheck_bridge_test.cpp](tests/unit/integration/legacy_bnetd/versioncheck_bridge_test.cpp) -- NEW (3 TEST_CASEs, 13 assertions).
- [tests/unit/integration/legacy_bnetd/CMakeLists.txt](tests/unit/integration/legacy_bnetd/CMakeLists.txt) -- 4 `pvpgn_v3_add_test(...)` entries.
- [Dockerfile.v3](Dockerfile.v3) -- 4 targets added to v3-build `--target` list; 4 invocations added to v3-test stage.

## Verify

```
docker build -f Dockerfile.v3 --target v3-test -t pvpgn-v3-test:r230 .
```

Per-test (all GREEN):

```
docker run --rm pvpgn-v3-test:r230 \
  /src/build/v3/tests/unit/integration/legacy_bnetd/test_integration_legacy_bnetd_runprog_bridge      --reporter compact
docker run --rm pvpgn-v3-test:r230 \
  /src/build/v3/tests/unit/integration/legacy_bnetd/test_integration_legacy_bnetd_news_bridge         --reporter compact
docker run --rm pvpgn-v3-test:r230 \
  /src/build/v3/tests/unit/integration/legacy_bnetd/test_integration_legacy_bnetd_userlog_bridge      --reporter compact
docker run --rm pvpgn-v3-test:r230 \
  /src/build/v3/tests/unit/integration/legacy_bnetd/test_integration_legacy_bnetd_versioncheck_bridge --reporter compact
```

Result: 12 TEST_CASEs, 54 assertions, all GREEN.

## Lessons (memorialised)

- Bridge `.cpp` files must be registered in the parent
  `integration_legacy_bnetd` library at `src/v3/CMakeLists.txt`,
  NOT in `integration_legacy_bnetd_linked`. The `_linked` lib is
  only for sources that need to reference `bnetd_legacy` headers
  via `setup_before.h` / `setup_after.h` brackets. Observation
  bridges have no such dependency.
- New tests must also be added in two places in `Dockerfile.v3`:
  the explicit `cmake --build --target ...` list AND the v3-test
  `RUN` chain.

