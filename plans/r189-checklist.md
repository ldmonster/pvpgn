# R189 checklist — strip legacy fallback in handle_bnet.cpp ads + realm-list handlers

> **Round:** R189 (follow-on to R188.b).
> **Parent plan:** [`plans/phase3b-handle-bnet-audit.md`](phase3b-handle-bnet-audit.md)
> **Scope:** Now that R188.b wired `install_ads_handlers()` + `install_realm_list_handler()` unconditionally, the `rc == -1 → fall through to legacy` branches in `handle_bnet.cpp` are statically unreachable under `PVPGN_V3_BNETD_INTEGRATION`. Make the v3 dispatchers **mandatory** (R168.a pattern): surface a startup-wiring bug as an error rather than silently using legacy.

## Changes

Four handlers in `src/bnetd/handle_bnet.cpp` restructured from
`#ifdef ... v3-try-then-fall-through ... #endif legacy-emit` to
`#ifdef ... v3-mandatory ... #else legacy-emit ... #endif`:

- [x] `_client_adreq` (~line 4670): `rc < 0` now logs error + returns -1
- [x] `_client_adclick2` (~line 4795): same pattern
- [x] `_client_realmlistreq` (~line 4095): same pattern
- [x] `_client_realmlistreq110` (~line 4160): same pattern

Net effect under v3:
- Legacy `AdBannerList.pick` / `AdBannerList.find` / `realmlist()` loops in these four handlers are no longer compiled (moved into `#else` branches).
- Removes the silent-no-op failure mode where a missing install_*() call would silently use legacy.
- Non-v3 (`PVPGN_BUILD_LEGACY` without `PVPGN_V3_BNETD_INTEGRATION`) builds keep identical behaviour.

## Verification

- [x] `get_errors` on `handle_bnet.cpp` — clean.
- [x] Dockerfile.v3 v3-test stage — `pvpgn-v3-test:r189` tagged, 174 "All tests passed".
- [ ] (Not exercisable in current CI) `Dockerfile.v3` v3-runtime stage uses `WITH_BNETD=OFF`, so `handle_bnet.cpp` is not recompiled. The edit is a refactor of pre-existing `#ifdef` blocks (mostly indentation shift + four new error-on-rc<0 branches + restructured `#else`); risk profile low. Verification would require WITH_BNETD=ON pipeline (out of scope; tracked under `plans/legacy-retirement-scope.md`).

## Out of scope / deferred

- R190 candidates: R188.c (anongame_lobby linked adapter), R188.d (WITH_D2CS=ON survey + d2cs handle_init relocation), R188.e (retire `pvpgn_v3_init_conn_apply_ex`).
- handle_bnet.cpp size: still ~6470 lines. Other handlers (`_client_motdw3`, `_client_realmjoinreq`, etc.) have similar fallback patterns — sweep them in a future round.
