# Phase 3.B — `handle_bnet.cpp` audit (R168 prep)

Status: **DRAFT / AUDIT**. Scope-only document; no code changes yet.

## 1. File at a glance

- Path: `src/bnetd/handle_bnet.cpp`
- Size: ~307 KB, ~6 470 lines, ~95 `_client_*` handler functions plus
  the central dispatch table `handle_bnet_packet`.
- This is the **single largest legacy handler** in the bnetd tree —
  roughly 10x the size of `handle_init.cpp` (which Phase 3.A is
  retiring).
- Cannot be retired in a single round. Phase 3.B is therefore split
  into the sub-rounds listed in §4.

## 2. Handler families

The `_client_*` functions group cleanly by concern. The audit below
maps each family to the v3 application module that already (or could)
own it, and the strangler bridge that already wraps it.

| Family | Legacy handlers (examples) | v3 application module | v3 bridge (existing) |
|--------|----------------------------|-----------------------|----------------------|
| Session bootstrap | `_client_compinfo1/2`, `_client_countryinfo1`, `_client_auth_info`, `_client_progident`, `_client_progident2` | `application/init`, `application/handshake` | `progident_dispatch_bridge`, `handshake_dispatch_bridge` |
| Account creation | `_client_createaccountw3`, `_client_createacctreq1/2` | `application/auth` (`create_account.hpp`) | `account_dispatch_bridge`, `send_createacct_reply_bridge` |
| Login (1.x / 1.09 / SRP / w3) | `_client_loginreq1/2`, `_client_loginreqw3`, `_client_authreq1`, `_client_authreq109`, `_client_logonproofreq` | `application/auth` (`login_user.hpp`, `login_classifier.hpp`) | `auth_dispatch_bridge`, `send_authreply1/109_bridge`, `send_logonproof_reply_bridge` |
| Password / e-mail / cdkey | `_client_changepassreq`, `_client_passchangereq`, `_client_passchangeproofreq`, `_client_cdkey/2/3`, `_client_changeemailreq`, `_client_setemailreply`, `_client_getpasswordreq` | `application/auth` (`change_password.hpp`) + new `email_management` module (gap) | `cdkey_dispatch_bridge`, `passemail_dispatch_bridge`, `send_passchange_bridge` |
| Ping / keepalive | `_client_pingreq`, `_client_echoreply`, `_client_udpok` | `application/keepalive` (exists) | `keepalive_dispatch_bridge` |
| Icon / file | `_client_iconreq`, `_client_fileinforeq` | `application/icons`, `application/file` (partial) | `file_dispatch_bridge`, `send_iconreply_bridge`, `send_fileinforeply_bridge` |
| Stats / profile / ladder | `_client_statsreq`, `_client_statsupdate`, `_client_profilereq`, `_client_playerinforeq`, `_client_ladderreq`, `_client_laddersearchreq` | `application/profile`, `application/ladder` (partial) | `profile_dispatch_bridge`, `ladder_dispatch_bridge` |
| Realm / character | `_client_realmlistreq`, `_client_realmlistreq110`, `_client_realmjoinreq109`, `_client_charlistreq`, `_client_changegameport`, `_client_mapauthreq1/2` | `application/realm` (partial) | `realm_dispatch_bridge`, `d2_character_dispatch_bridge`, `gameport_dispatch_bridge` |
| Friends | `_client_friendslistreq`, `_client_friendinforeq`, `_client_atfriendscreen`, `_client_atinvitefriend`, `_client_atacceptinvite`, `_client_atacceptdeclineinvite` | `application/friends` (exists) | `friends_dispatch_bridge`, `send_friendslist_bridge`, `send_friendinfo_bridge`, `send_at*` |
| Channel / chat | `_client_joinchannel`, `_client_leavechannel`, `_client_message` | `application/channel` (partial), `application/chat` | `message_dispatch_bridge`, `channel_state_bridge`, `send_chatevent_compose_*` |
| Game lifecycle | `_client_gamelistreq`, `_client_joingame`, `_client_startgame1/3/4`, `_client_closegame`, `_client_gamereport` | `application/game` (partial) | `gamelist_join_bridge`, `startgame_bridge`, `game_report_bridge`, `send_startgame*ack_bridge` |
| Ads | `_client_adreq`, `_client_adack`, `_client_adclick`, `_client_adclick2` | `application/ads` (gap — small) | `ad_dispatch_bridge`, `send_adreply_bridge`, `send_adclick2reply_bridge` |
| Clan | `_client_clanmemberlistreq`, `_client_clan_motdreq/chg`, `_client_clan_createreq`, `_client_clan_createinvitereq/reply`, `_client_clan_disbandreq`, `_client_clanmember_rankupdatereq/removereq`, `_client_clan_membernewchiefreq`, `_client_clan_invitereq/reply`, `_client_claninforeq` | `application/clan` (partial) | `clan_dispatch_bridge`, `clan_send_bridge`, `clan_bridges`, `send_clan_*` |
| Misc / telemetry | `_client_motdw3`, `_client_crashdump`, `_client_readmemory`, `_client_extrawork`, `_client_unknown_1b/2b/39`, `_client_regsnoopreply` | `application/telemetry`, `application/stub` | `telemetry_dispatch_bridge`, `stub_dispatch_bridge`, `send_motdw3_bridge` |

## 3. Gap analysis

Modules that already exist and have full v3 application coverage:
`auth` (login/logout/create/change_password), `keepalive`, `friends`,
`chat`, `init`.

Modules **with v3 bridges but no application-layer logic** (still
delegate to legacy):

- `application/email_management` — not yet present; covers
  `_client_changeemailreq`, `_client_setemailreply`,
  `_client_getpasswordreq`. **R170 work-unit candidate.**
- `application/ads` — bridges exist (`ad_dispatch_bridge`,
  `send_adreply_bridge`, `send_adclick2reply_bridge`) but the
  business logic still lives in the legacy switch. **R171.**
- `application/realm` — partial; `realmlistreq110` decoding still
  legacy. **R172.**
- `application/game/anongame_lobby` — gameport / startgame variants
  still split between v3 and legacy. **R173.**

## 4. Phase 3.B sub-rounds (proposed)

| Round | Scope | Risk | Tests added |
|-------|-------|------|-------------|
| R169 | Phase 3.A close-out (handle_init retirement) — prerequisite, not part of 3.B | low | wire dispatch into legacy switch return path |
| R170 | `application/email_management` skeleton + bridge wiring for `changeemail` / `setemail` / `getpassword` | medium (touches DB/storage paths) | 6–8 dispatch tests |
| R171 | `application/ads` application-layer logic; retire `_client_ad*` legacy branches under `PVPGN_V3_BNETD_INTEGRATION` | low (read-only paths) | 4 ad-cycle tests |
| R172 | Full `realmlistreq110` decoding + reply composition in v3; retire legacy realm branches | medium (wire format) | 6 realm-listing tests |
| R173 | Game lifecycle (`startgame*`, `gamereport`, `closegame`) — careful; lobby state crosses connection boundary | **HIGH** | 10+ lifecycle tests, requires anongame harness |
| R174 | Clan-create / clan-invite full v3 ownership | medium | 8 clan tests |
| R175 | Misc cleanup (`_client_unknown_*`, `_client_crashdump`, `_client_readmemory`, `_client_extrawork`) — delete or move to telemetry | low | 3 stub tests |
| R176 | `_client_message` chat-command parser → `application/chat/command_router` | medium | 8 command tests |
| R177 | Remove the `handle_bnet_packet` central switch under
        `#ifdef PVPGN_V3_BNETD_INTEGRATION` (analogous to Phase 3.A) | medium | smoke |
| R178 | Delete `handle_bnet.cpp` from CMake when single-binary v3 mode is the only mode | low (build only) | – |

Total: **9 sub-rounds (R170 → R178)** to fully retire
`handle_bnet.cpp`. Each round is independently verifiable via the
Dockerfile.v3 test stage.

## 5. Cross-cutting concerns

- **Snapshot lifetime** (see `plans/snapshot-lifetime-scope.md`):
  every new application module must adopt the
  `prefs_v3::get_snapshot()` pattern from R166; no raw
  `prefs.get_*()` strings stored beyond a single dispatch.
- **Realmlist gating** (R168.c): now lives in `dispatch_init_conn`
  via `InitConnRequest::d2cs_ip_allowed`. R172 must keep the
  decision authoritative and not re-implement the IP filter locally.
- **Mandatory handlers** (R168.a pattern): every new
  `install_*_handler` site must reject (return -1) on null handler
  rather than fall through to legacy. Pattern proven in
  `init_conn_bridge.cpp`.

## 6. Prerequisites before starting R170

1. R169 (Phase 3.A) green — legacy `handle_init` switch retired.
2. Dockerfile.v3 `v3-runtime` preview stage at green (currently
   soft-fails — see R168 changelog).
3. `application/email_management` skeleton agreed (interface only)
   in a small prep round.

## 7. Out-of-scope for Phase 3.B

- WoL / IRC dispatchers (already in `wol_dispatch_bridge` /
  `irc_dispatch_bridge`; covered separately by Phase 3.C).
- Telnet admin FSM (Phase 3.D — already prototyped under
  `protocol/telnet/admin_fsm`).
- D2CS-side handlers (`handle_d2cs.cpp` equivalent does not exist;
  Phase 4 covers `d2cs_legacy` retirement).

---
*Authored R168. Author notes go in changelog.md.*

## R169.c follow-up: server.cpp packet-pump strangler

R169 attempted to drop `src/bnetd/handle_init.cpp` from the
`bnetd_legacy` CMake sources entirely. That is blocked because the
file still defines `handle_init_packet`, which is called from
`src/bnetd/server.cpp` packet pump (see `server.cpp:992`).

Prerequisites before R169.c can land:
- Add a v3 bridge for the packet-pump dispatch step that today
  reads `packet_get_type(packet) == CLIENT_INITCONN` and forwards
  to `handle_init_packet`. Either: (a) move the entire `cclass`
  byte read + v3 dispatch into a new `protocol/bnet/init/`
  module + `extern "C" pvpgn_v3_init_packet_handle`, or
  (b) keep `handle_init_packet` as a 3-line shim that only does
  `cclass = packet_get_d2cs_client(packet)->cclass;` and calls
  `pvpgn_v3_init_conn_apply_ex(...)`.
- Option (b) is simpler -- after R169.b the body is already only
  the v3 dispatch + the unreachable-fallback log. We can shrink
  the legacy file to a 30-line shim now and defer full deletion.

Recommended sequencing:
- R171.x (after R170 lands handle_bnet skeleton): shrink
  `handle_init.cpp` to a 30-line shim, keep file present but
  trivial. Add a parity test that the shim calls _apply_ex once.
- R175.x (when server.cpp packet pump is itself stranglered):
  delete `handle_init.cpp` + `handle_init.h` entirely.

## R170 progress (R170.a..f completed)

- R170.a -- `application/email_management` dispatcher landed.
  `dispatch_email_change` covers both the set-when-unset and
  replace-existing paths; `dispatch_password_recovery` covers the
  `_client_getpasswordreq` validation. 13 unit cases pass (set,
  replace, case-insensitive match, invalid email shapes,
  feature-disabled, no-stored-email-as-rejected).
- R170.b -- `application/ads` skeleton (interface-only headers
  `ad_pick.hpp` + placeholder TU `ad_pick.cpp`). Real impl deferred
  to R171.
- R170.c -- WAS scoped as "scaffold application/realm" but the
  module already exists with 11 headers under
  `src/v3/application/realm/include/application/realm/`
  (create_character, delete_character, list_characters, load/save,
  join_game_server, gs_queue, character_list_repository,
  character_lock, character_persistence, d2_ladder_repository).
  R172's "realm" work is therefore implementation gap-filling, not
  scaffolding.
- R170.d -- `application/anongame_lobby` skeleton (interface-only
  header `lobby.hpp` + placeholder TU `lobby.cpp`). Defines
  `LobbyEntry`, `LobbyAdmitRequest/Response`,
  `LobbyAdmitStatus { kQueued, kPromoted, kDuplicate, kRejected }`.
  Real impl deferred to R173.
- R170.e -- `src/bnetd/handle_init.cpp` shrunk from ~247 lines to
  105 lines (a 30-line v3 shim + the GPL header + the !v3 build
  guard). Under `PVPGN_V3_BNETD_INTEGRATION` the file is now just
  packet validation + a single call to `pvpgn_v3_init_conn_apply_ex`.
  Under !v3 the file fails with `#error` because `prefs_v3_shim.h`
  already requires the v3 bridge.
- R170.f -- this audit doc updated. R171 starter chosen: **ads**
  (smallest remaining gap module; only 4 legacy handlers; read-only
  data flow; 4 dispatch tests). R172 = realm gap-filling, R173 =
  anongame_lobby implementation.

Updated round assignment:
- R171 = ads impl (was already this in audit).
- R172 = realm gap-filling (NEW -- realm scaffold already exists).
- R173 = anongame_lobby impl (NEW round, displaces game lifecycle).
- R174 = game lifecycle (was R173).
- R175 = clan create/invite (was R174).
- R176 = misc cleanup (was R175).
- R177 = chat command parser (was R176).
- R178 = handle_bnet central switch retirement (was R177).
- R179 = delete handle_bnet.cpp (was R178).
- Plus follow-up R171.x: shrink handle_init.cpp -- DONE in R170.e.
- R175.x (delete handle_init.cpp + .h) still pending.
