# R188 checklist — wire `install_ads_handlers()` + `install_realm_list_handler()`

> **Round:** R188.b (renamed from "findadreq" — no such symbol existed).
> **Parent plan:** [`plans/phase3b-handle-bnet-audit.md`](phase3b-handle-bnet-audit.md)
> **Scope:** Activate the v3 ads + realm-list handlers that were authored in R171.c-linked + R172.d-linked but whose `install_*()` call sites were never actually wired into `src/bnetd/server.cpp` (progress-master log was inaccurate).

## Findings

- `install_ads_handlers()` declared in `src/v3/integration/legacy_bnetd/include/integration/legacy_bnetd/install_v3_handlers.hpp:50`, defined at `install_v3_handlers.cpp:366` — **never called**.
- `install_realm_list_handler()` declared at `install_v3_handlers.hpp:58`, defined at `install_v3_handlers.cpp:381` — **never called**.
- `src/bnetd/server.cpp:2080-2099` calls `install_send_packet_handler()` + `install_init_conn_apply_handler()` (also `install_change_password_handler()` / `install_login_user_handler()` slightly earlier) under `PVPGN_V3_BNETD_INTEGRATION`. The ads + realm installers belong right next to them.
- Consequence in current main: `pvpgn_v3_ads_pick_apply` / `pvpgn_v3_ads_click_apply` / `pvpgn_v3_realm_list_apply` always return -1 → handle_bnet falls through to legacy `AdBannerList.pick` / `realmlist()` loop. The R171.e / R172.d "v3-authoritative" claim is currently a no-op.

## Tasks

- [x] Audit: grep `install_ads_handlers` / `install_realm_list_handler` across `src/` → only declaration + definition sites found.
- [x] Add `install_ads_handlers()` and `install_realm_list_handler()` to the unconditional installer block in `src/bnetd/server.cpp` (under `PVPGN_V3_BNETD_INTEGRATION`).
- [x] Build via `Dockerfile.v3` `v3-test` + `v3-runtime` stages; confirm 174 "All tests passed" lines (no test count change — we are wiring already-tested code into the runtime).
- [ ] (Deferred to R189) After confirming the v3 path is live, strip the `// rc == -1: no handler installed; fall through to legacy.` branches in `handle_bnet.cpp` `_client_adreq` / `_client_adclick2` / `_client_realmlistreq*`.
- [x] Update `plans/progress-master.md` with the R188 entry.
- [x] Append `changelog.md`.

## Out of scope

- R188.c (anongame_lobby linked-half adapter) — selected by user but R188.b alone fits this round's verification budget. Carry-over.
- R188.d (WITH_D2CS=ON survey) — still gated.
- R188.e (retire `pvpgn_v3_init_conn_apply_ex`) — still gated on `PVPGN_V3_BNETD_INTEGRATION` retirement.
