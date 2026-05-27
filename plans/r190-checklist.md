# R190 — handle_bnet.cpp v3 fallback-pattern sweep (AUDIT-ONLY)

**Status:** ✅ Complete (audit-only, no code changes).
**Predecessor:** R189 (made `_client_adreq` / `_client_adclick2` / `_client_realmlistreq` / `_client_realmlistreq110` v3-mandatory).
**Scope (chosen via dialog):** Sweep the remaining ~145 `#ifdef PVPGN_V3_BNETD_INTEGRATION` blocks in `src/bnetd/handle_bnet.cpp` for additional candidates that should follow the R168.a / R189 "mandatory v3, fail-loud" transform.

## Method

1. Grepped `#ifdef PVPGN_V3_BNETD_INTEGRATION` in `src/bnetd/handle_bnet.cpp` → 145 hits.
2. Grepped `pvpgn_v3_*_apply(` (the R189 mandatory contract) → 7 hits = 3 ABI declarations + 4 call sites, all already converted in R189.
3. Grepped `v3rc` + `pvpgn_v3_send_*` short-circuit patterns → ~20 send-adapter call sites.
4. Read `src/v3/integration/legacy_bnetd/include/integration/legacy_bnetd/install_v3_handlers.hpp` to confirm which installers are wired in `server.cpp`.

## Findings

The 145 `#ifdef` blocks fall into three disjoint categories:

### Category A — mandatory-`_apply()` contract  (4 sites, **all already converted in R189**)
- `_client_adreq`           → `pvpgn_v3_ads_pick_apply`     (R189)
- `_client_adclick2`        → `pvpgn_v3_ads_click_apply`    (R189)
- `_client_realmlistreq`    → `pvpgn_v3_realm_list_apply`   (R189)
- `_client_realmlistreq110` → `pvpgn_v3_realm_list_apply`   (R189)

**No further A-category candidates exist** — these 4 handlers are the entire population that uses the "v3 *replaces* the legacy work; rc<0 means installer missing" contract.

### Category B — observation-only `(void)pvpgn_v3_*_dispatch_try(...)`  (majority of remaining hits)
Examples: `_client_unknown_1b`, `_client_compinfo1`, `_client_compinfo2`, `_client_countryinfo1`, every handler with a `clan_dispatch_try` / `friends_dispatch_try` / `auth_dispatch_try` / `keepalive_dispatch_try` / `realm_dispatch_try` / `account_dispatch_try` / `profile_dispatch_try` / `ladder_dispatch_try` / `d2_character_dispatch_try` / `telemetry_dispatch_try` / `ad_dispatch_try` / `progident_dispatch_try` / `gameport_dispatch_try` / `cdkey_dispatch_try` / `passemail_dispatch_try` / `stub_dispatch_try` / `handshake_dispatch_try` call, etc.

**Contract:** these `_dispatch_try` ABIs fire metrics / telemetry / parallel-execution side-effects, then return. Return value is deliberately ignored (`(void)` cast). Legacy work proceeds unconditionally regardless of return value.

**Verdict:** these are **not candidates** for the R189 transform. They are correctly written as-is.

### Category C — best-effort send-adapter short-circuits  (~20 sites)
Two sub-patterns:
- `if (pvpgn_v3_send_X(...) <= 0) { ...legacy packet build... }` — v3 best-effort send; falls back to legacy build on failure.
- `int v3rc = pvpgn_v3_send_X(...); if (v3rc == 1) { ...return 0... }` — v3 succeeded → skip legacy; v3rc ≤ 0 → legacy runs.

Sites (representative, not exhaustive):
- `_client_compinfo1` / `_client_compinfo2` → `pvpgn_v3_send_compreply` / `pvpgn_v3_send_sessionkey{1,2}`
- `_client_pingreq` (~L3057) → `pvpgn_v3_send_pingreply`
- `_client_echoreq`-like (~L1029) → `pvpgn_v3_send_echoreq`
- `_client_authinfo` (~L1095) → `pvpgn_v3_send_authinfo_reply`
- `_client_createacct{,2}` (~L1384, L1452) → `pvpgn_v3_send_createacctreply{1,2}`
- `_client_statsreq` (~L2299) → `pvpgn_v3_send_statsreply`
- `_client_loginreq{1,2,_w3}` (~L2458, L2663, L2804) → `pvpgn_v3_send_loginreply{1,2,_w3}`
- `_client_changepass{,proof}` (~L2932, L3026) → `pvpgn_v3_send_passchangereply` / `pvpgn_v3_send_passchangeproofreply`
- `_client_logonproof` (~L3192) → `pvpgn_v3_send_logonproof_reply`
- `_client_friendslistreq` (~L3371) → `pvpgn_v3_send_friendslistreply`

**Contract:** the `pvpgn_v3_send_*` ABIs are thin packet-builder + send-bridge wrappers. The underlying `pvpgn_v3_send_packet_try` bridge IS wired (`install_send_packet_handler()` at `server.cpp:2089`). However, each individual send adapter can still return `<= 0` for legitimate reasons unrelated to installer wiring:
- packet allocation failed (`packet_create()` returned nullptr in v3),
- the v3 builder rejected the input (e.g. a malformed `v3_msg`),
- the connection is in a state where v3 prefers to defer to legacy.

In those cases, the legacy `if ((rpacket = packet_create(...)))` fallback is a **robustness feature**, not a wiring oversight. Removing it would convert "v3 declined to send this particular reply" into "legacy fails the connection" — a regression in fault-tolerance, not a R168.a-style failure mode.

**Verdict:** these are **not candidates** for the R189 transform under the current contract. A separate, larger project would be needed to:
1. Audit each `pvpgn_v3_send_*` adapter for whether `<= 0` can mean anything other than "v3 declined" (e.g., promote `packet_create()` failure to the central send-packet bridge so the per-adapter contract simplifies to `rc == 1 success / rc == 0 declined`).
2. Define a new installer contract that gates the adapter's existence rather than its per-call success.
3. Only then is "mandatory v3, return -1 if not installed" applicable.

That is a Phase 4 / Phase 5 project, not an R190 follow-on.

## Result

- **Code changes:** none.
- **Build verification:** none required (no code touched).
- **Tests:** unchanged; v3-test image from R189 (`pvpgn-v3-test:r189`, 174 "All tests passed") remains current.
- **Knowledge captured:** this checklist documents *why* no further mandatory transforms are safe under the existing ABI contracts, preventing future rounds from re-discovering the same conclusion.

## Recommended next rounds (deferred)

- **R191 (recommended next):** R188.c — anongame_lobby linked-half adapter. Real new code: `IAnonGameLobbyRepository` over `anongame_queue` global, wire `_client_findanongame` / `_client_anongame_search`. ~200 lines.
- **R192 candidate:** R188.d — WITH_D2CS=ON survey. Investigate the gated d2cs pipeline.
- **R193 candidate:** R188.e — retire `pvpgn_v3_init_conn_apply_ex` + `install_legacy_init_conn_apply_handler`. Gated on `anongame_infos.cpp` cutover.
- **R194 (Phase-4 scope, NOT a quick round):** harden `pvpgn_v3_send_*` adapter contract per "Category C verdict" above; then re-evaluate which sites can become mandatory.

## Verification artefacts

- Grep transcripts (this conversation): 145 `#ifdef PVPGN_V3_BNETD_INTEGRATION` hits, 7 `pvpgn_v3_*_apply(` hits (4 call-sites all R189-converted), 20+ `pvpgn_v3_send_*` short-circuit sites.
- Installers wired in `src/bnetd/server.cpp`: `install_change_password_handler` (L2063), `install_login_user_handler` (L2078), `install_send_packet_handler` (L2089), `install_init_conn_apply_handler` (L2099), plus R188.b additions `install_ads_handlers` + `install_realm_list_handler` (post-L2099).
