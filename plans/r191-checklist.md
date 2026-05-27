# R191 Checklist — Recovery of R188.b installer wiring

Started: 2026-05-27.

## Background

Session-start audit (post R188.b/R189/R190) found the recorded progress diverged from
the actual git state of `src/bnetd/server.cpp`:

- `git status --short` → `src/bnetd/server.cpp` **NOT modified**.
- `grep install_ads_handlers|install_realm_list_handler src/bnetd/server.cpp` → **0 matches**.
- The R188.b entry in `plans/progress-master.md` claims those two installer
  calls were added at `server.cpp:~2099`. They were not.
- R189 then promoted four `handle_bnet.cpp` handlers to the mandatory
  `rc < 0 → eventlog_error + return -1` contract:
  - `_client_adreq` (~L4670)
  - `_client_adclick2` (~L4795)
  - `_client_realmlistreq` (~L4095)
  - `_client_realmlistreq110` (~L4160)
- Because the installers are never called, `pvpgn_v3_ads_pick_apply` /
  `pvpgn_v3_ads_click_apply` / `pvpgn_v3_realm_list_apply` return -1 in
  every production build with `PVPGN_V3_BNETD_INTEGRATION + WITH_BNETD=ON`.
- Net effect: every ad request / ad click / realm-list request would fail.

Why CI missed it: `Dockerfile.v3` builds with `WITH_BNETD=OFF`, so
`src/bnetd/server.cpp` is not compiled in the v3-test stage. The bug only
materialises in a `WITH_BNETD=ON` production build.

## R191.a — Fix the missing installer wiring

- [x] Re-confirm regression via `grep_search` on `src/bnetd/server.cpp`
      (only 4 `install_*` matches — none for ads or realm_list).
- [x] Verify the v3 lib still defines / declares the functions:
  - `install_v3_handlers.hpp:50` → `void install_ads_handlers();`
  - `install_v3_handlers.hpp:58` → `void install_realm_list_handler();`
  - `install_v3_handlers.cpp:367` → `void install_ads_handlers() { ... }`
  - `install_v3_handlers.cpp:382` → `void install_realm_list_handler() { ... }`
- [x] Add a new "Step 4 / E.4 (R188.b recovery)" block in
      `src/bnetd/server.cpp` directly after the existing
      `install_init_conn_apply_handler()` call (~L2099) inside the
      existing `#ifdef PVPGN_V3_BNETD_INTEGRATION` region:
  ```cpp
  pvpgn::integration::legacy_bnetd::install_ads_handlers();
  pvpgn::integration::legacy_bnetd::install_realm_list_handler();
  ```
- [x] Header `integration/legacy_bnetd/install_v3_handlers.hpp` already
      included since R168 (server.cpp:126) — no new include required.
- [x] `get_errors src/bnetd/server.cpp` → clean.
- [x] Update `plans/progress-master.md` with the R191.a entry that
      truthfully documents the recovery (instead of silently rewriting
      R188.b).

## Verification

- `get_errors`: clean (IntelliSense).
- Docker v3-test rebuild: **no-op**. `WITH_BNETD=OFF` does not compile
  `server.cpp`, so the v3-test image is byte-identical to R189/R190.
  This is the SAME LIMITATION that allowed the original R188.b regression
  to be claimed-done-without-being-done — it cannot detect the fix
  either.
- A `WITH_BNETD=ON` build is needed to confirm. The local Windows MSVC
  build still hits the pre-existing `strings.h` issue in
  `src/common/tag.cpp` (unrelated). Adding a `WITH_BNETD=ON` legacy
  Linux CI lane is recommended (see "Follow-ups" below).

## Risk profile

- Trivial structural edit: two extra calls in an existing namespace
  block alongside four pre-existing siblings.
- All four `install_*_handler()` functions follow the same idempotent
  CAS-on-nullptr pattern (`std::call_once` / atomic compare-exchange),
  so re-running them on a SIGHUP-restart is safe.
- Zero behaviour change for non-`PVPGN_V3_BNETD_INTEGRATION` builds
  (entirely under the existing `#ifdef`).

## Follow-ups (carry-over to R192+)

- [ ] R192 candidate: add a `WITH_BNETD=ON` CI lane to `Dockerfile.v3`
      (or `appveyor.yml`) so future R168-style wiring regressions are
      caught at build time. The same bug class hit R168→R169 (init_conn
      installer missing) and R171/R172→R191.a (ads + realm-list
      installers missing).
- [ ] R188.c (anongame_lobby linked-half adapter) — still DEFERRED.
      Data-model mismatch (v3 `LobbyEntry` flat vs legacy
      `matchlists[queue][level]` 2D buckets) requires Phase-3-scope
      design work before scaffolding.
- [ ] R188.d (`WITH_D2CS=ON` survey) — still DEFERRED.
- [ ] R188.e (retire `pvpgn_v3_init_conn_apply_ex`) — still DEFERRED,
      blocked on retargeting `handle_init.cpp` to `feed_with_side_effects`.

## Lessons

- "Progress-master entry claims X" is not evidence that X happened.
  Always `grep`-verify recent installer / wiring edits at session start
  when they fall under `WITH_BNETD=ON`-only code paths that the
  default CI pipeline cannot touch.
- A `WITH_BNETD=ON` CI lane is a much-needed defensive measure for
  Phase 3 strangler work.
