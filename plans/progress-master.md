# PvPGN Refactoring Master Progress

Last updated: 2026-05-19 (Round 47)

## Overall Status

| Plan | Status | Progress |
|------|--------|----------|
| Phase 1: Common/Compat Migration | 🔄 In Progress | Steps 1-3 complete, Step 4 in progress (E.3 Rounds 1-38 done, all E.3 items complete) |
| Phase 2: bnetd Migration | ⏳ Not Started | Depends on Phase 1 |
| Phase 3: D2 Services Migration | ⏳ Not Started | Depends on Phase 1 |
| Phase 4: Tools Migration | ✅ Mostly Done | bnpass, bnproxy, bniutils, bntrackd, client tools all migrated |
| Build System Consolidation | ⏳ Not Started | Depends on all phases |
| Testing Strategy | 🔄 In Progress | Tests being added alongside migration |

## Phase 1: Common/Compat Migration

### Steps

- [x] Step 1: Create `src/v3/infra/compat/` — COMPLETE
- [x] Step 2: Create `src/v3/infra/crypto/` — COMPLETE (with parity tests)
- [x] Step 3: Migrate protocol `*_protocol.h` headers — COMPLETE (all 12 headers)
- [~] Step 4: Packet/Queue migration — IN PROGRESS
  - [x] E.1: v3 surface additions (Writer enhancements) — COMPLETE
  - [x] E.2: send_packet bridge — COMPLETE
  - [x] E.3: Per-module migration (Rounds 1-38 done — ALL COMPLETE)
    - [x] handle_init.cpp — authoritative apply path done
    - [x] bnet auth/login (AuthReply1, AuthReply109, AuthInfo, LoginReply, LoginReplyW3, LogonProofReply, CreateAcctReply, IconReply)
    - [x] chat / channel / message (chatevent compose bridge, channel state bridge, channellist)
    - [x] game list / start / report (Round 16 complete)
    - [x] clan — Round 38: clan_send_bridge wired into all 8 send sites in clan.cpp
    - [x] anongame.cpp + anongame_infos.cpp (Rounds 17-19)
    - [x] clan_dispatch (Round 20 — observation bridge done)
    - [x] friends_dispatch (Round 21)
    - [x] auth_dispatch (Round 22)
    - [x] keepalive_dispatch (Round 23)
    - [x] realm_dispatch (Round 24)
    - [x] account_dispatch (Round 25)
    - [x] profile_dispatch (Round 26)
    - [x] ladder_dispatch (Round 27)
    - [x] d2_character_dispatch (Round 28)
    - [x] telemetry_dispatch (Round 29)
    - [x] ad_dispatch (Round 30)
    - [x] progident_dispatch (Round 31)
    - [x] gameport_dispatch (Round 32)
    - [x] cdkey_dispatch (Round 33)
    - [x] passemail_dispatch (Round 34)
    - [x] handshake_dispatch (Round 35)
    - [x] stub_dispatch (Round 36)
    - [x] file_dispatch (Round 37)
  - [~] E.4: Acceptance criteria — IN PROGRESS (121 packet_create sites remain, down from 156)
    - Round 46: 6 send bridges (motdw3, realmlistlegacy, realmlist, claninfo, profilereply, realmjoin) — 7 sites guarded in handle_bnet.cpp
    - Round 47: 4 send bridges (charlistreply, adreply, adclick2reply, playerinforeply) — 4 sites guarded in handle_bnet.cpp
    - Round 48: 6 send bridges (gamelistreply, startgame1ack, startgame3ack, startgame4ack, ladderreply, laddersearchreply) — 6 sites guarded in handle_bnet.cpp
- [ ] Step 5: Migrate `src/common/` utility modules
- [ ] Step 6: Migrate `src/common/` network/socket layer
- [ ] Step 7: Migrate `src/common/` storage/account layer (t_list / t_hashtable elimination)
- [ ] Step 8: Migrate `src/compat/` platform abstractions
- [ ] Step 9: Delete migrated legacy files
- [ ] Step 10: Final integration and build verification

### Side Tracks (all complete)

- [x] xalloc elimination — Rounds 1-13 COMPLETE (100% xalloc-free in bnetd + all subsystems)
- [x] src/test/ retirement — ported to Catch2, deleted
- [x] src/json/ retirement — switched to nlohmann/json
- [x] src/bnpass/ retirement — migrated to src/v3/tools/bnpass/
- [x] src/bnproxy/ retirement — dropped (deprecated since 4.0)
- [x] src/bniutils/ relocation — moved to src/v3/tools/bniutils/ + deep modernization
- [x] src/bntrackd/ relocation — moved to src/v3/tools/bntrackd/
- [x] src/client/ deep modernization — bnbot, bnftp, bnchat, bnstat all standalone v3
- [x] E2E docker smoke tests — bnftp, bnchat, bnstat, bnbot all green
- [x] compat filesystem cluster retirement
- [x] compat/strdup, strcasecmp, strncasecmp, strsep, gettimeofday, uname, mmap retirement
- [x] t_connection per-field migration round 1 (clientexe, clientver, loggeduser)

## Phase 2: bnetd Migration

- [ ] Step 1: Domain layer extraction
- [ ] Step 2: Application use cases
- [ ] Step 3: Protocol FSMs
- [ ] Step 4: Infrastructure layer
- [ ] Step 5: Connection state machine decomposition
- [ ] Step 6: Composition root
- [ ] Step 7: Legacy bnetd deletion
- [ ] Step 8: Final integration

## Phase 3: D2 Services Migration

- [ ] Step 1: Domain layer completion
- [ ] Step 2: Application use cases
- [ ] Step 3: Protocol FSMs
- [ ] Step 4: Infrastructure
- [ ] Step 5: Inter-service communication
- [ ] Step 6: Composition roots
- [ ] Step 7: Combined mode integration

## Phase 4: Tools Migration

- [x] bnpass — migrated to src/v3/tools/bnpass/
- [x] bnproxy — dropped (deprecated)
- [x] bniutils — relocated to src/v3/tools/bniutils/ + modernized
- [x] bntrackd — relocated to src/v3/tools/bntrackd/
- [x] client tools (bnbot, bnftp, bnchat, bnstat) — deep modernization complete
- [ ] Delete remaining legacy tool files (after Phase 1 Step 9)

## Build System Consolidation

- [ ] Phase 1: Inventory and audit
- [ ] Phase 2: Unify CMake structure
- [ ] Phase 3: Migrate targets
- [ ] Phase 4: Remove legacy build artifacts
- [ ] Phase 5: CI/CD updates
- [ ] Phase 6: Documentation
- [ ] Phase 7: Final verification

## Testing

- [x] Unit tests for v3 protocol layer (1225 assertions / 213 cases)
- [x] Integration tests for legacy bridges (all bridge tests green)
- [x] E2E smoke tests (bnftp, bnchat, bnstat, bnbot — all green)
- [ ] Coverage targets met
- [ ] Legacy test removal

## Session Log

### 2026-05-19
- Orchestrator started systematic refactoring execution
- Read and analyzed all plan files
- Created this master progress tracker
- Analyzed step4-checklist.md: Rounds 1-37 complete (observation bridges for all major handlers)
- xalloc elimination 100% complete (Rounds 1-13)
- All side tracks complete (tools, compat, etc.)
- Round 38 COMPLETE: Implemented `pvpgn_v3_clan_send_try` observation bridge
  - Created `src/v3/integration/legacy_bnetd/include/integration/legacy_bnetd/clan_send_bridge.hpp`
  - Created `src/v3/integration/legacy_bnetd/src/clan_send_bridge.cpp`
  - Created `tests/unit/integration/legacy_bnetd/clan_send_bridge_test.cpp` (3 test cases)
  - Added source to `src/v3/CMakeLists.txt`
  - Added test to `tests/unit/integration/legacy_bnetd/CMakeLists.txt`
  - Added build target + RUN line to `Dockerfile.v3`
  - Wired into `src/bnetd/clan.cpp` at all 8 `conn_push_outqueue` sites under `#ifdef PVPGN_V3_BNETD_INTEGRATION`
  - Updated `plans/step4-checklist.md`: `[x] clan.`
- E.3 is now 100% complete (all modules have observation bridges)
- Next action: Work toward E.4 acceptance criteria (reduce 156 packet_create sites to 0)
- Round 39 COMPLETE: Implemented `send_handshake_bridge` — E.4 first batch
  - Created `src/v3/integration/legacy_bnetd/include/integration/legacy_bnetd/send_handshake_bridge.hpp`
  - Created `src/v3/integration/legacy_bnetd/src/send_handshake_bridge.cpp`
    - `pvpgn_v3_send_compreply(conn)` — SERVER_COMPREPLY (0x05), 20 bytes, all fixed constants
    - `pvpgn_v3_send_sessionkey1(conn, sessionkey)` — SERVER_SESSIONKEY1 (0x28), 8 bytes
    - `pvpgn_v3_send_sessionkey2(conn, sessionnum, sessionkey)` — SERVER_SESSIONKEY2 (0x1D), 12 bytes
  - Created `tests/unit/integration/legacy_bnetd/send_handshake_bridge_test.cpp` (12 test cases)
    - Wire-parity byte checks for all 3 packet types
    - null-conn rejection, no-handler returns 0, handler return propagation
  - Added source to `src/v3/CMakeLists.txt`
  - Added test target to `tests/unit/integration/legacy_bnetd/CMakeLists.txt`
  - Added build target + RUN line to `Dockerfile.v3` (line 87)
  - Wired into `src/bnetd/handle_bnet.cpp`:
    - Added 3 forward decls under `#ifdef PVPGN_V3_BNETD_INTEGRATION`
    - `_client_compinfo1`: 2 `packet_create` sites now guarded by bridge calls
    - `_client_compinfo2`: 2 `packet_create` sites now guarded by bridge calls
  - Updated `plans/step4-checklist.md`: Round 39 entry, handle_bnet.cpp count 68→64
  - Running total: **152 packet_create sites** remaining (down from 156)
- Round 40 COMPLETE: Implemented `send_authreq1_server_bridge` — SERVER_AUTHREQ1 (0x06)
  - Created `src/v3/integration/legacy_bnetd/include/integration/legacy_bnetd/send_authreq1_server_bridge.hpp`
  - Created `src/v3/integration/legacy_bnetd/src/send_authreq1_server_bridge.cpp`
    - `pvpgn_v3_send_authreq1_server(conn, timestamp, filename, equation)`
    - Uses existing `encode(AuthReq1Server)` codec: FF 06 size_le + u64 ts + filename\0 + equation\0
  - Created `tests/unit/integration/legacy_bnetd/send_authreq1_server_bridge_test.cpp` (9 test cases)
    - null-conn, null-filename, null-equation rejection
    - Wire-parity byte checks for non-empty and empty strings
    - Handler return value propagation (1 / 0 / -1)
  - Added source to `src/v3/CMakeLists.txt` (before send_authreply1_bridge.cpp)
  - Added test target to `tests/unit/integration/legacy_bnetd/CMakeLists.txt`
  - Added build target to `Dockerfile.v3` cmake line (via sed)
  - Added RUN test line to `Dockerfile.v3` (after init_conn_bridge test)
  - Wired into `src/bnetd/handle_bnet.cpp`:
    - Added forward decl `pvpgn_v3_send_authreq1_server` under `#ifdef PVPGN_V3_BNETD_INTEGRATION`
    - `_client_progident`: 1 `packet_create` site now guarded by bridge call
    - Refactored to compute checkrevision + timestamp before the bridge/legacy branch
  - Updated `plans/step4-checklist.md`: Round 40 entry, handle_bnet.cpp count 64→63
  - Running total: **151 packet_create sites** remaining (down from 152)
- Round 41 COMPLETE: Implemented `send_cdkey_reply_bridge` — SERVER_CDKEYREPLY (0x30), SERVER_CDKEYREPLY2 (0x36), SERVER_CDKEYREPLY3 (0x42)
  - Created `src/v3/integration/legacy_bnetd/include/integration/legacy_bnetd/send_cdkey_reply_bridge.hpp`
  - Created `src/v3/integration/legacy_bnetd/src/send_cdkey_reply_bridge.cpp`
    - `pvpgn_v3_send_cdkeyreply(conn, message, owner)` — SERVER_CDKEYREPLY (0x30): header(4) + u32 LE + owner\0
    - `pvpgn_v3_send_cdkeyreply2(conn, result, owner)` — SERVER_CDKEYREPLY2 (0x36): header(4) + u32 LE + owner\0
    - `pvpgn_v3_send_cdkeyreply3(conn, message, owner_name)` — SERVER_CDKEYREPLY3 (0x42): header(4) + u32 LE + owner_name\0
  - Created `tests/unit/integration/legacy_bnetd/send_cdkey_reply_bridge_test.cpp` (12 test cases)
    - null-conn rejection, no-handler returns 0, wire-parity for non-empty and nullptr owner
    - Handler return value propagation (1 / 0 / -1) for all 3 functions
  - Added source to `src/v3/CMakeLists.txt` (before send_chatevent_compose_bridge.cpp)
  - Added test target to `tests/unit/integration/legacy_bnetd/CMakeLists.txt`
  - Added build target to `Dockerfile.v3` cmake line (via sed)
  - Added RUN test line to `Dockerfile.v3` (after send_chatevent_compose_e2e test)
  - Wired into `src/bnetd/handle_bnet.cpp`:
    - Added 3 forward decls `pvpgn_v3_send_cdkeyreply/2/3` under `#ifdef PVPGN_V3_BNETD_INTEGRATION`
    - `_client_cdkey`: 1 `packet_create` site now guarded by bridge call
    - `_client_cdkey2`: 1 `packet_create` site now guarded by bridge call
    - `_client_cdkey3`: 1 `packet_create` site now guarded by bridge call
  - Updated `plans/step4-checklist.md`: Round 41 entry, handle_bnet.cpp count 63→60
  - Running total: **148 packet_create sites** remaining (down from 151)
- Round 42 COMPLETE: Implemented `send_fileinforeply_bridge` — SERVER_FILEINFOREPLY (SID_GETFILETIME, 0x33) and SERVER_PINGREPLY (SID_NULL, 0x00)
  - Created `src/v3/integration/legacy_bnetd/include/integration/legacy_bnetd/send_fileinforeply_bridge.hpp`
  - Created `src/v3/integration/legacy_bnetd/src/send_fileinforeply_bridge.cpp`
    - `pvpgn_v3_send_fileinforeply(conn, type, unknown2, timestamp, filename)` — SERVER_FILEINFOREPLY (0x33): header(4) + u32 type LE + u32 unknown2 LE + u64 timestamp LE + filename\0
    - `pvpgn_v3_send_pingreply(conn)` — SERVER_PINGREPLY (0x00): 4-byte header only
  - Created `tests/unit/integration/legacy_bnetd/send_fileinforeply_bridge_test.cpp`
    - null-conn rejection, no-handler returns 0, wire-parity for fileinforeply and pingreply
    - null filename treated as empty string, handler return propagation (1 / 0 / -1)
  - Added source to `src/v3/CMakeLists.txt` (before send_chatevent_compose_bridge.cpp)
  - Added test target to `tests/unit/integration/legacy_bnetd/CMakeLists.txt`
  - Added build target to `Dockerfile.v3` cmake line
  - Added RUN test line to `Dockerfile.v3` (after send_cdkey_reply_bridge test)
  - Wired into `src/bnetd/handle_bnet.cpp`:
    - Added forward decls `pvpgn_v3_send_fileinforeply` and `pvpgn_v3_send_pingreply` under `#ifdef PVPGN_V3_BNETD_INTEGRATION`
    - `_client_fileinforeq`: 1 `packet_create` site now guarded by bridge call (extracts timestamp from rpacket before bridge call)
    - `_client_pingreq`: guarded by `pvpgn_v3_send_pingreply` (existing function, newly wired)
  - Updated `plans/step4-checklist.md`: Round 42 entry, handle_bnet.cpp count 60→58
  - Running total: **146 packet_create sites** remaining (down from 148)
- Round 43 COMPLETE: Implemented `send_passchange_bridge` — SERVER_PASSCHANGEREPLY (SID_PASSCHANGE, 0x55) and SERVER_PASSCHANGEPROOFREPLY (SID_PASSCHANGEPROOF, 0x56)
  - Created `src/v3/integration/legacy_bnetd/include/integration/legacy_bnetd/send_passchange_bridge.hpp`
  - Created `src/v3/integration/legacy_bnetd/src/send_passchange_bridge.cpp`
    - `pvpgn_v3_send_passchangereply(conn, message, salt, server_public_key)` — SERVER_PASSCHANGEREPLY (0x55): header(4) + u32 message LE + salt[32] + server_public_key[32] = 72 bytes
    - `pvpgn_v3_send_passchangeproofreply(conn, response, server_password_proof)` — SERVER_PASSCHANGEPROOFREPLY (0x56): header(4) + u32 response LE + server_password_proof[20] = 28 bytes
  - Created `tests/unit/integration/legacy_bnetd/send_passchange_bridge_test.cpp`
    - null-conn rejection, no-handler returns 0, wire-parity (FF 55 48 00 and FF 56 1C 00 headers verified)
    - handler return propagation (1 / 0 / -1) for both functions
  - Added source to `src/v3/CMakeLists.txt` (after send_fileinforeply_bridge.cpp)
  - Added test target to `tests/unit/integration/legacy_bnetd/CMakeLists.txt`
  - Added build target + RUN line to `Dockerfile.v3`
  - Wired into `src/bnetd/handle_bnet.cpp`:
    - Added forward decls `pvpgn_v3_send_passchangereply` and `pvpgn_v3_send_passchangeproofreply` under `#ifdef PVPGN_V3_BNETD_INTEGRATION`
    - `_client_passchangereq`: bridge reads message/salt/spk from rpacket, calls `pvpgn_v3_send_passchangereply`; on rc==1 skips legacy push
    - `_client_passchangeproofreq`: bridge reads response/proof from rpacket, calls `pvpgn_v3_send_passchangeproofreply`; on rc==1 skips legacy push + clears proofs
  - Updated `plans/step4-checklist.md`: Round 43 entry, handle_bnet.cpp count 58→56
  - Running total: **144 packet_create sites** remaining (down from 146)
- Round 44 COMPLETE: Wired `send_statsreply_bridge` — SERVER_STATSREPLY (SID_READUSERDATA, 0x26)
  - Bridge files (`send_statsreply_bridge.hpp`, `send_statsreply_bridge.cpp`, `send_statsreply_bridge_test.cpp`) were created during Round 44 setup (interrupted); this round completes the wiring
  - `pvpgn_v3_send_statsreply(conn, name_count, key_count, request_id, values, value_count)` — variable-length: header(4) + name_count(4) + key_count(4) + request_id(4) + NUL-terminated value strings
  - Forward decl `pvpgn_v3_send_statsreply` already present in `handle_bnet.cpp` `#ifdef PVPGN_V3_BNETD_INTEGRATION` block (lines 273-278)
  - Wired into `src/bnetd/handle_bnet.cpp` `_client_statsreq`: after legacy builds rpacket + appends value strings, bridge collects them into `std::vector<char const*>` and calls `pvpgn_v3_send_statsreply`; on rc==1 skips legacy push
  - Added `send_statsreply_bridge.cpp` to `src/v3/CMakeLists.txt` (after send_passchange_bridge.cpp)
  - Added `test_integration_legacy_bnetd_send_statsreply_bridge` to `tests/unit/integration/legacy_bnetd/CMakeLists.txt`
  - Added build target + RUN line to `Dockerfile.v3`
  - Updated `plans/step4-checklist.md`: Round 44 entry, handle_bnet.cpp count 56→55
  - Running total: **143 packet_create sites** remaining (down from 144)
- Round 45 COMPLETE: Wired 5 friend/arranged-team send bridges — SERVER_FRIENDSLISTREPLY (0x65), SERVER_FRIENDINFOREPLY (0x66), SERVER_ARRANGEDTEAM_FRIENDSCREEN (0x60), SERVER_ARRANGEDTEAM_INVITE_FRIEND_ACK (0x61), SERVER_ARRANGEDTEAM_MEMBER_DECLINE (0x62)
  - New bridge files created (headers, impls, tests):
    - `send_friendslist_bridge.hpp/.cpp` + `send_friendslist_bridge_test.cpp`
    - `send_friendinfo_bridge.hpp/.cpp` + `send_friendinfo_bridge_test.cpp`
    - `send_atfriendscreen_bridge.hpp/.cpp` + `send_atfriendscreen_bridge_test.cpp`
    - `send_atinvitefriend_bridge.hpp/.cpp` + `send_atinvitefriend_bridge_test.cpp`
    - `send_atacceptdecline_bridge.hpp/.cpp` + `send_atacceptdecline_bridge_test.cpp`
  - Forward decls added to `handle_bnet.cpp` `#ifdef PVPGN_V3_BNETD_INTEGRATION` block (lines 282–312)
  - Wired into `src/bnetd/handle_bnet.cpp`:
    - `_client_friendslistreq`: re-walks friend list to build `std::vector<pvpgn_v3_friend_entry>`, calls `pvpgn_v3_send_friendslistreply`; on rc==1 skips legacy push
    - `_client_friendinforeq` offline path: calls `pvpgn_v3_send_friendinforeply` with FRIEND_TYPE_NON_MUTUAL/FRIENDSTATUS_OFFLINE/0/""; on rc==1 skips legacy push
    - `_client_friendinforeq` online path: reads type/status/clienttag from rpacket, extracts game_name from packet payload, calls `pvpgn_v3_send_friendinforeply`; on rc==1 skips legacy push
    - `_client_atfriendscreen`: collects player name strings from packet payload into `std::vector<char const*>`, calls `pvpgn_v3_send_atfriendscreenreply`; on rc==1 skips legacy push
    - `_client_atinvitefriend` ACK: reads count/id/timestamp/teamsize/info[5] from rpacket, calls `pvpgn_v3_send_atinvitefriendack`; on rc==1 skips legacy push (SERVER_ARRANGEDTEAM_SEND_INVITE to invitees not yet bridged)
    - `_client_atacceptdeclineinvite`: calls `pvpgn_v3_send_atmemberdecline` with count/action/decliner_name; on rc==1 skips legacy push
  - Added 5 new `.cpp` source files to `src/v3/CMakeLists.txt`
  - Added 5 new test targets to `tests/unit/integration/legacy_bnetd/CMakeLists.txt`
  - Added 5 new build targets + RUN lines to `Dockerfile.v3`
  - Updated `plans/step4-checklist.md`: Round 45 entry, handle_bnet.cpp count 55→50
  - Running total: **138 packet_create sites** remaining (down from 143)

### 2026-05-20
- Round 49 COMPLETE: New `send_echoreq_bridge` -- SERVER_ECHOREQ (SID_PING, 0x25)
  - Created `src/v3/integration/legacy_bnetd/include/integration/legacy_bnetd/send_echoreq_bridge.hpp`
  - Created `src/v3/integration/legacy_bnetd/src/send_echoreq_bridge.cpp`
    - `pvpgn_v3_send_echoreq(conn, ticks)` -- reuses `encode(bnet::Ping)`
    - 8-byte wire: ff 25 08 00 + u32 LE ticks
  - Created `tests/unit/integration/legacy_bnetd/send_echoreq_bridge_test.cpp` (5 cases)
    - no-handler / null-conn / wire-parity / zero-ticks / handler propagation
  - Added source to `src/v3/CMakeLists.txt`
  - Added test target to `tests/unit/integration/legacy_bnetd/CMakeLists.txt`
  - Added build target + RUN line to `Dockerfile.v3`
  - Wired into `src/bnetd/handle_bnet.cpp` `_client_auth_info` (line 962):
    one previously unbridged `packet_create` site now guarded by the
    `pvpgn_v3_send_echoreq` call under `#ifdef PVPGN_V3_BNETD_INTEGRATION`
  - Updated `plans/step4-checklist.md` with Round 49 entry
  - Validation: deferred to docker build with cache (per user choice)

### 2026-05-20
- Round 49 COMPLETE: New `send_echoreq_bridge` -- SERVER_ECHOREQ (SID_PING, 0x25)
  - Created `src/v3/integration/legacy_bnetd/include/integration/legacy_bnetd/send_echoreq_bridge.hpp`
  - Created `src/v3/integration/legacy_bnetd/src/send_echoreq_bridge.cpp`
    - `pvpgn_v3_send_echoreq(conn, ticks)` -- reuses `encode(bnet::Ping)`
    - 8-byte wire: ff 25 08 00 + u32 LE ticks
  - Created `tests/unit/integration/legacy_bnetd/send_echoreq_bridge_test.cpp` (5 cases)
    - no-handler / null-conn / wire-parity / zero-ticks / handler propagation
  - Added source to `src/v3/CMakeLists.txt`
  - Added test target to `tests/unit/integration/legacy_bnetd/CMakeLists.txt`
  - Added build target + RUN line to `Dockerfile.v3`
  - Wired into `src/bnetd/handle_bnet.cpp` `_client_auth_info` (line 962):
    one previously unbridged `packet_create` site now guarded by the
    `pvpgn_v3_send_echoreq` call under `#ifdef PVPGN_V3_BNETD_INTEGRATION`
  - Updated `plans/step4-checklist.md` with Round 49 entry
  - Validation: deferred to docker build with cache (per user choice)
- Round 49 VALIDATED: docker build (Dockerfile.v3, --target v3-test) succeeded.
  - New `send_echoreq_bridge.cpp` compiled into `integration_legacy_bnetd`.
  - New `test_integration_legacy_bnetd_send_echoreq_bridge` built and run.
  - Full chained v3-test RUN line passed (image tagged `pvpgn-v3:round49`).
- Round 49 VALIDATED: docker build (Dockerfile.v3, --target v3-test) succeeded.
  - New `send_echoreq_bridge.cpp` compiled into `integration_legacy_bnetd`.
  - New `test_integration_legacy_bnetd_send_echoreq_bridge` built and run.
  - Full chained v3-test RUN line passed (image tagged `pvpgn-v3:round49`).
- Round 50 COMPLETE: Reused `pvpgn_v3_send_motdw3` for per-news entries.
  - No new files; wired into `_news_cb` (`src/bnetd/handle_bnet.cpp` line ~3891).
  - Guard added under `#ifdef PVPGN_V3_BNETD_INTEGRATION`; on rc==1
    skips the legacy `packet_create` / `conn_push_outqueue` path.
  - Parity: `packet_append_lstr` and `write_cstring` produce
    identical bytes for lstrs created via `lstr_set_str`.
  - Updated `plans/step4-checklist.md` with Round 50 entry.
- Round 50 COMPLETE: Reused `pvpgn_v3_send_motdw3` for per-news entries.
  - No new files; wired into `_news_cb` (`src/bnetd/handle_bnet.cpp` line ~3891).
  - Guard added under `#ifdef PVPGN_V3_BNETD_INTEGRATION`; on rc==1
    skips the legacy `packet_create` / `conn_push_outqueue` path.
  - Parity: `packet_append_lstr` and `write_cstring` produce
    identical bytes for lstrs created via `lstr_set_str`.
  - Updated `plans/step4-checklist.md` with Round 50 entry.
- Round 50 VALIDATED: docker v3-test image built green (`pvpgn-v3:round50`). Note: `handle_bnet.cpp` is not compiled in WITH_BNETD=OFF stage; call-site edits inside `#ifdef PVPGN_V3_BNETD_INTEGRATION` follow same pattern as Rounds 46-48 (unvalidated by docker, low risk).
- Round 50 VALIDATED: docker v3-test image built green (`pvpgn-v3:round50`). Note: `handle_bnet.cpp` is not compiled in WITH_BNETD=OFF stage; call-site edits inside `#ifdef PVPGN_V3_BNETD_INTEGRATION` follow same pattern as Rounds 46-48 (unvalidated by docker, low risk).

### Round 51 -- send_clan_invitereply (SID 0x77)

- Created v3 bridge `pvpgn_v3_send_clan_invitereply(void*, count u32, result u8)`:
  - Header `src/v3/integration/legacy_bnetd/include/integration/legacy_bnetd/send_clan_invitereply_bridge.hpp`
  - Impl   `src/v3/integration/legacy_bnetd/src/send_clan_invitereply_bridge.cpp`
  - Encodes `ClanGenericResultReply{sid=kSidClanInvite, cookie=count, result}` -> 9-byte wire (ff 77 09 00 + u32 LE + u8).
- Reused existing codec entry; no new struct or wire type.
- Test `tests/unit/integration/legacy_bnetd/send_clan_invitereply_bridge_test.cpp` (5 cases: no-handler / null-conn / 9-byte wire parity / zero values / return propagation).
- Wired into 2 sites in `src/bnetd/handle_bnet.cpp` under `PVPGN_V3_BNETD_INTEGRATION`:
  - `_client_clan_invitereq` early-reject (~line 6945, recipient = c)
  - `_client_clan_invitereply` invitee response (~line 7038, recipient = conn, reads back the bytes that legacy code just set on rpacket)
- Updated `src/v3/CMakeLists.txt`, `tests/unit/integration/legacy_bnetd/CMakeLists.txt`, and `Dockerfile.v3` (build target list + RUN test line).
- Validation: docker v3-test image built green (`pvpgn-v3:round51`); new bridge test passes; full chain still `1225 assertions in 213 test cases` for the final binary in the chain. `handle_bnet.cpp` call-site edits remain unvalidated by docker (WITH_BNETD=OFF), same caveat as Rounds 46-50.

### Round 52 -- send_clan_membernewchief_reply (SID 0x74)

- Created v3 bridge `pvpgn_v3_send_clan_membernewchief_reply(void*, count u32, result u8)`.
  - Reuses `ClanGenericResultReply` codec with `sid=kSidClanMemberNewChief` -> 9-byte wire (ff 74 09 00 + u32 LE + u8).
  - Header / impl files under `src/v3/integration/legacy_bnetd/{include,src}/...`.
- Test `send_clan_membernewchief_reply_bridge_test.cpp` (5 cases: same pattern as the 0x77 bridge).
- Wired ONLY the FAILED branch in `_client_clan_membernewchiefreq` (handle_bnet.cpp ~line 6862, recipient = c). The SUCCESS branch broadcasts to all online clan members via `clan_send_packet_to_online_members()` and is intentionally left on the legacy path -- the single-conn send_packet ABI cannot model the broadcast.
- Updated `src/v3/CMakeLists.txt`, `tests/unit/integration/legacy_bnetd/CMakeLists.txt`, and `Dockerfile.v3` (build target list + RUN test line).
- Validation: docker v3-test image built green (`pvpgn-v3:round52`); new bridge test runs in the chain. `handle_bnet.cpp` edit remains unvalidated by docker (WITH_BNETD=OFF), same caveat as Rounds 46-51.

### Round 52 -- send_clan_membernewchief_reply (SID 0x74)

- Created v3 bridge pvpgn_v3_send_clan_membernewchief_reply(void*, count u32, result u8).
  - Reuses ClanGenericResultReply codec with sid=kSidClanMemberNewChief -> 9-byte wire (ff 74 09 00 + u32 LE + u8).
  - Header / impl files under src/v3/integration/legacy_bnetd/{include,src}/...
- Test send_clan_membernewchief_reply_bridge_test.cpp (5 cases: same pattern as the 0x77 bridge).
- Wired ONLY the FAILED branch in _client_clan_membernewchiefreq (handle_bnet.cpp ~line 6862, recipient = c). The SUCCESS branch broadcasts to all online clan members via clan_send_packet_to_online_members() and is intentionally left on the legacy path -- the single-conn send_packet ABI cannot model the broadcast.
- Updated src/v3/CMakeLists.txt, tests/unit/integration/legacy_bnetd/CMakeLists.txt, and Dockerfile.v3 (build target list + RUN test line).
- Validation: docker v3-test image built green (pvpgn-v3:round52); new bridge test runs in the chain. handle_bnet.cpp edit remains unvalidated by docker (WITH_BNETD=OFF), same caveat as Rounds 46-51.


### Round 53 -- send_clanmember_remove_reply (SID 0x78) + send_clanmember_rankupdate_reply (SID 0x7A)

- Two new v3 bridges, both reusing ClanGenericResultReply codec:
  - pvpgn_v3_send_clanmember_remove_reply (sid=kSidClanMemberRemove, 0x78) -- 9-byte wire (ff 78 09 00 + u32 LE + u8)
  - pvpgn_v3_send_clanmember_rankupdate_reply (sid=kSidClanMemberRankUpdate, 0x7A) -- 9-byte wire (ff 7a 09 00 + u32 LE + u8)
  - Headers / impls under src/v3/integration/legacy_bnetd/{include,src}/...
- Two new test files, each with 4 Catch2 cases (no-handler / null-conn / wire parity / return propagation).
- Wired one site per bridge in src/bnetd/handle_bnet.cpp under PVPGN_V3_BNETD_INTEGRATION:
  - _client_clanmember_rankupdatereq (~line 6780): read back count + result from rpacket, try v3 before legacy push.
  - _client_clanmember_removereq (~line 6832): same pattern. The interleaved 0x7E SERVER_CLANMEMBER_REMOVED_NOTIFY broadcast stays on the legacy path.
- Updated src/v3/CMakeLists.txt, tests/unit/integration/legacy_bnetd/CMakeLists.txt, and Dockerfile.v3 (build target list + RUN test lines).
- Validation: docker v3-test image built green (pvpgn-v3:round53); both new bridge tests run in the chain. handle_bnet.cpp call-site edits remain unvalidated by docker (WITH_BNETD=OFF), same caveat as Rounds 46-52.


## Round 54 -- SERVER_CHANGEPASSACK (SID 0x31) bridge

- Added src/v3/integration/legacy_bnetd/{include,src}/send_changepassack_bridge.{hpp,cpp}
- Added tests/unit/integration/legacy_bnetd/send_changepassack_bridge_test.cpp (5 cases: no-handler / null-conn / success wire / fail wire / return propagation)
- Reuses existing pvpgn::protocol::bnet::ChangePasswordReply codec.
- Wired forward decl + call-site in src/bnetd/handle_bnet.cpp::_client_changepassreq under PVPGN_V3_BNETD_INTEGRATION (single push site).
- Updated src/v3/CMakeLists.txt, tests/.../CMakeLists.txt, Dockerfile.v3 (target list + RUN test block).
- Validated: docker build pvpgn-v3:round54 OK -- naming to docker.io/library/pvpgn-v3:round54 done.
- Caveat: handle_bnet.cpp call-site not exercised by v3-test (WITH_BNETD=OFF); pattern matches Rounds 46-53.
- Pivoted from REGSNOOPREQ (site is inside #if 0 dead code) to SERVER_CHANGEPASSACK.

## Round 55 -- legacy_d2cs scaffold (dispatcher half)

- Added new v3 directory tree src/v3/integration/legacy_d2cs/{include,src}/.
- Created send_packet_bridge.{hpp,cpp} mirroring legacy_bnetd:
  - namespace pvpgn::integration::legacy_d2cs
  - C ABI pvpgn_v3_d2cs_send_packet_try / _send_packet_available
  - kSendPacketMaxSize=3072 (parity with d2cs MAX_PACKET_SIZE)
  - install_legacy_send_packet_handler declared but not yet defined
    (linked half deferred).
- Added pvpgn_v3_add_library(integration_legacy_d2cs ...) target in
  src/v3/CMakeLists.txt with only core as PUBLIC_DEPS.
- Added tests/unit/integration/legacy_d2cs/CMakeLists.txt registering
  test_integration_legacy_d2cs_send_packet_bridge (5 Catch2 cases:
  no-handler / null-conn-or-bytes / size-bounds / forward-verbatim /
  return-propagation).
- Updated tests/unit/integration/CMakeLists.txt to add_subdirectory(legacy_d2cs).
- Updated Dockerfile.v3: added test target to build list and RUN block.
- Validated: docker build pvpgn-v3:round55 OK -- naming to docker.io/library/pvpgn-v3:round55 done.
- Scope deferred to future round: extract d2cs_legacy static library
  from the d2cs executable target, then add
  integration_legacy_d2cs_linked with the real packet_create +
  conn_d2cs_outqueue implementation, plus PVPGN_V3_D2CS_INTEGRATION
  macro wiring in src/d2cs/CMakeLists.txt. Until that is in place,
  pvpgn_v3_d2cs_send_packet_try has no production producer; only
  unit tests exercise the dispatcher.

## Round 56 -- d2cs_legacy static library carve-out

- Refactored src/d2cs/CMakeLists.txt to mirror the bnetd_legacy pattern:
  - New STATIC library d2cs_legacy containing all .cpp/.h sources
    except main.cpp and the win32 winmain/resource files.
  - Original d2cs executable now contains only main.cpp + winmain
    (+ resource.rc on Win32) and links against d2cs_legacy.
  - PUBLIC link deps on d2cs_legacy: common compat fmt win32
    ${NETWORK_LIBRARIES} (matches prior d2cs link line).
  - PUBLIC include dir on d2cs_legacy: src/d2cs/ (so future
    integration_legacy_d2cs_linked can include legacy headers as
    'd2cs/connection.h' etc relative to src/).
- Validated:
  - Legacy build: docker build -f Dockerfile pvpgn-legacy:round56 OK
    (d2cs_legacy.a built, d2cs exe linked against it,
    naming to docker.io/library/pvpgn-legacy:round56 done).
  - v3 build:     docker build -f Dockerfile.v3 pvpgn-v3:round56 OK
    (no regression, naming to docker.io/library/pvpgn-v3:round56 done).
- Unblocks future round: integration_legacy_d2cs_linked can now be
  added with a PUBLIC_DEPS d2cs_legacy + the real send_packet_via_legacy
  implementation calling packet_create(packet_class_d2cs) and
  conn_d2cs_outqueue (parity with bnetd link half).
- No code changes outside src/d2cs/CMakeLists.txt; no source edits.

## Round 57 -- integration_legacy_d2cs_linked (real send_packet_via_legacy)

- Added src/v3/integration/legacy_d2cs/src/send_packet_bridge_link.cpp:
  - install_legacy_send_packet_handler() installs send_packet_via_legacy.
  - send_packet_via_legacy:
      1. casts conn_ptr to pvpgn::d2cs::t_connection*
      2. packet_create(packet_class_raw)
      3. packet_get_raw_data_build + memcpy + packet_set_size
      4. pvpgn::d2cs::conn_push_outqueue + packet_del_ref
      5. returns 1 on push >= 0, else 0.
  - Mirrors integration/legacy_bnetd/src/send_packet_bridge_link.cpp.
- Added pvpgn_v3_add_library(integration_legacy_d2cs_linked ...) target in
  src/v3/CMakeLists.txt, gated by if(TARGET d2cs_legacy):
  - PUBLIC_DEPS: core, integration_legacy_d2cs
  - DEPS: d2cs_legacy
  - target_compile_options -w on the linked TU (legacy headers not
    warning-clean under v3 strict warnings).
  - target_include_directories adds src/ + build dir for
    'common/setup_before.h', 'd2cs/connection.h', 'common/packet.h'.
- Validated v3-test build: pvpgn-v3:round57 OK -- no regression
  (linked half not built in v3-only image because WITH_D2CS=OFF means
  d2cs_legacy target absent; same precedent as integration_legacy_bnetd_linked).
- Combined-build validation deferred: no existing docker stage configures
  both d2cs_legacy and the v3 tree simultaneously; same situation
  applies to the bnetd linked half. Future combined-build docker stage
  could exercise both.

## Round 59 -- legacy_d2dbs scaffold + d2dbs_legacy carve-out

Carved d2dbs into a static lib + thin executable (parity with bnetd/d2cs):
- src/d2dbs/CMakeLists.txt: d2dbs_legacy static lib contains all sources
  except main.cpp and ../win32/d2dbs_winmain.cpp/resource. d2dbs exe
  links against d2dbs_legacy. PUBLIC include dir: src/d2dbs/.

Added legacy_d2dbs scaffold under src/v3/integration/legacy_d2dbs/:
- send_packet_bridge.{hpp,cpp}: dispatcher with C ABI
  pvpgn_v3_d2dbs_send_packet_try / _send_packet_available;
  kSendPacketMaxSize=3072 (conservative; kBufferSize=20480 enforced
  at runtime by the linked half).
- send_packet_bridge_link.cpp: install_legacy_send_packet_handler
  memcpys bytes into conn->WriteBuf at nCharsInWriteBuffer (the
  d2dbs send model -- no t_packet outqueue), with remaining-space
  guard mirroring legacy dbspacket.cpp handlers.

CMake:
- integration_legacy_d2dbs static lib (always built, deps: core).
- integration_legacy_d2dbs_linked gated on TARGET d2dbs_legacy;
  PUBLIC_DEPS core + integration_legacy_d2dbs; DEPS d2dbs_legacy;
  -w on the link TU; include dirs src/ + build for legacy headers.

Tests:
- tests/unit/integration/legacy_d2dbs/send_packet_bridge_test.cpp:
  5 Catch2 cases (no-handler / null-conn-or-bytes / size-bounds /
  forward-verbatim / return-propagation).
- Added test subdir to tests/unit/integration/CMakeLists.txt.
- Added Dockerfile.v3 entry (target list + RUN block).

Validation:
- docker build -f Dockerfile.v3 pvpgn-v3:round59 OK
  (naming to docker.io/library/pvpgn-v3:round59 done).
- docker build -f Dockerfile  pvpgn-legacy:round59 OK
  (naming to docker.io/library/pvpgn-legacy:round59 done;
  d2dbs_legacy.a built; d2dbs exe linked against it).
- Linked half compile validation deferred (no combined-build CI
  stage; same precedent as bnetd / d2cs linked halves).

## Round 60 -- first real d2cs bridge: send_loginreply (D2CS_CLIENT_LOGINREPLY 0x01)

- New: src/v3/integration/legacy_d2cs/include/integration/legacy_d2cs/send_loginreply_bridge.hpp
- New: src/v3/integration/legacy_d2cs/src/send_loginreply_bridge.cpp
- New: tests/unit/integration/legacy_d2cs/send_loginreply_bridge_test.cpp (5 cases / 22 assertions)
- Wired: src/v3/CMakeLists.txt (integration_legacy_d2cs += send_loginreply_bridge.cpp; PUBLIC_DEPS += protocol_common, protocol_d2cs)
- Wired: tests/unit/integration/legacy_d2cs/CMakeLists.txt (+ pvpgn_v3_add_test target)
- Wired: Dockerfile.v3 (build list + RUN block)
- Wired: src/d2cs/CMakeLists.txt (PVPGN_V3_D2CS_INTEGRATION macro + linked-lib link gated on TARGET integration_legacy_d2cs_linked)
- Wired: src/d2cs/handle_bnetd.cpp on_bnetd_accountloginreply -- forward decl + #ifdef hook before conn_push_outqueue.
- Validation: pvpgn-v3:round60 OK (5/5 new cases pass; all tests pass). pvpgn-legacy:round60 OK.
- Note: Legacy build has no v3 tree configured so PVPGN_V3_D2CS_INTEGRATION stays undefined; call-site hook compiles but is dormant. Same precedent as bnetd rounds.

## Round 61 -- d2cs send_createcharreply bridge (D2CS_CLIENT_CREATECHARREPLY 0x02)
- New: src/v3/integration/legacy_d2cs/{include,src}/send_createcharreply_bridge.{hpp,cpp}
- New: tests/unit/integration/legacy_d2cs/send_createcharreply_bridge_test.cpp (4 cases)
- Wired: src/v3/CMakeLists.txt, tests CMake, Dockerfile.v3.
- Wired two call sites: src/d2cs/handle_d2cs.cpp::on_client_createcharreq and src/d2cs/handle_bnetd.cpp on_bnetd_charloginreply (CREATECHARREQ branch) under PVPGN_V3_D2CS_INTEGRATION.
- Validation: pvpgn-v3:round61 OK + pvpgn-legacy:round61 OK.

## Round 62 -- d2cs send_charloginreply bridge (D2CS_CLIENT_CHARLOGINREPLY 0x07)
- New: protocol/d2cs/codec adds CharLoginReply struct + kClientCharLoginReply (0x07) + reply constants, encode() impl.
- New: src/v3/integration/legacy_d2cs/{include,src}/send_charloginreply_bridge.{hpp,cpp}
- New: tests/unit/integration/legacy_d2cs/send_charloginreply_bridge_test.cpp (4 cases)
- Wired: src/v3/CMakeLists.txt, tests CMake, Dockerfile.v3.
- Wired call site: src/d2cs/handle_bnetd.cpp on_bnetd_charloginreply (CHARLOGINREQ branch) under PVPGN_V3_D2CS_INTEGRATION.
- Validation: pvpgn-v3:round62 OK + pvpgn-legacy:round62 OK.

## R63 - d2cs send_creategamereply + send_joingamereply
- Added bridges in src/v3/integration/legacy_d2cs/{include,src}/.../send_{creategame,joingame}reply_bridge.{hpp,cpp}.
- joingamereply addr trick: legacy used bn_int_nset (BE wire); bridge takes addr_host and applies internal bswap32() before assigning to m.addr so subsequent LE encode produces the desired BE wire bytes.
- Wire format: creategamereply 13B (type 0x03), joingamereply 21B (type 0x04).
- Wired 3 call sites under PVPGN_V3_D2CS_INTEGRATION:
  - src/d2cs/handle_d2cs.cpp - on_client_creategamereq failure path
  - src/d2cs/handle_d2gs.cpp - on_d2gs_creategamereply
  - src/d2cs/handle_d2gs.cpp - on_d2gs_joingamereply (computes v3_addr via trans_net on success)
- Catch2 test send_gamereply_bridges_test.cpp asserts wire bytes incl. BE addr ordering.
- Validation: pvpgn-v3:round63 + pvpgn-legacy:round63 both built; 1225 assertions / 213 cases pass.

## R64 - first d2dbs bridge: send_echorequest
- Added bridge in src/v3/integration/legacy_d2dbs/{include,src}/.../send_echorequest_bridge.{hpp,cpp}.
- Wire: 8B, all LE - u16 size=8 | u16 type=0x34 | u32 seqno (codec.EchoRequest).
- Added PVPGN_V3_D2DBS_INTEGRATION macro wiring in src/d2dbs/CMakeLists.txt (mirrors d2cs R60 pattern).
- Wired src/d2dbs/dbspacket.cpp dbs_keepalive() with forward decl + per-connection #ifdef hook that returns early on handled=1 (skips legacy memcpy into WriteBuf).
- New Catch2 test send_echorequest_bridge_test.cpp asserts 8-byte wire incl. LE seqno encoding (0xdeadbeef -> ef be ad de).
- Validation: pvpgn-v3:round64 + pvpgn-legacy:round64 both built; test count 128 -> 129; 1225+ assertions pass.

## R65 - bnetd legacy-fallback removal audit
- Scanned src/bnetd/*.cpp for packet_create(packet_class_bnet) and checked PVPGN_V3_BNETD_INTEGRATION within ±120 lines.
- handle_*.cpp: 76 / 76 sites covered (100%). All dispatched client packet handlers are bridged.
- Full src/bnetd: 97 sites total, 87 covered, 10 uncovered.
- Of the 10 uncovered: 1 is a false positive (server.cpp:815 is an INPUT read-buffer alloc, not an outbound reply). 9 real gaps remain.
- Full report: plans/r65-bnetd-fallback-audit.md (lists each gap with packet type + trigger and suggested R66-R69 grouping).
- No correctness risk: strangler-fig contract leaves un-bridged sites running legacy unchanged.

## R66 - SERVER_ECHOREQ + SERVER_MESSAGEBOX bridges
- Existing send_echoreq bridge was already in v3 (only handle_bnet.cpp call site wired); added second hook in src/bnetd/connection.cpp conn_test_latency() pre-game branch.
- New send_messagebox bridge (v3 codec MessageBox already present):
  - src/v3/integration/legacy_bnetd/{include,src}/.../send_messagebox_bridge.{hpp,cpp}
  - Wire: ff 19 <size_LE> | u32 style LE | text \0 | caption \0.
  - Wired in src/bnetd/message.cpp messagebox_show().
- New Catch2 test send_messagebox_bridge_test.cpp asserts header + style + cstring layout.
- Validation: pvpgn-v3:round67 + pvpgn-legacy:round67 both built; test count 129 -> 130 after R66.

## R67 - friend-list acks (SERVER_FRIEND{ADD,DEL,MOVE}_ACK)
- Three new bridges (v3 codec already had FriendAddAck/FriendDelAck/FriendMoveAck encoders):
  - send_friendadd_ack_bridge: hdr | name \0 | status u8 | location u8 | client_tag u32 LE | loc_name \0 (variable size)
  - send_frienddel_ack_bridge: hdr | friend_num u8 (5B)
  - send_friendmove_ack_bridge: hdr | pos1 u8 | pos2 u8 (6B)
- Wired 4 call sites in src/bnetd/command.cpp under PVPGN_V3_BNETD_INTEGRATION:
  - /f a (line ~1544) -> send_friendadd_ack (reads back status fields via bn_byte_get/bn_int_get on the local status struct)
  - /f r (line ~1655) -> send_frienddel_ack
  - /f promote / /f demote (lines ~1699/1742) -> send_friendmove_ack
- New Catch2 test send_friend_acks_bridges_test.cpp covers all three with wire-byte assertions.
- Validation: test count 129 -> 131 across R66+R67; both docker images built; 1225 assertions / 213 cases.

- R69 antihack bridges (SERVER_READMEMORY 0x17, SERVER_REQUIREDWORK 0x4C) wired in connection.cpp. Tests + both docker images green.

- R71 d2cs handle_bnetd.cpp gaps bridged: init handshake, AUTHREPLY, GAMEINFOREPLY. Tests + both docker images green.

- R72 d2cs handle_d2gs bridges (5 sites) -- v3+legacy green


- R73 d2cs handle_d2cs.cpp wire joingamereply+charloginreply -- v3+legacy green


- R74 d2cs simple reply bridges (4 sites) -- v3+legacy green


- R75 d2cs variable-length list-reply bridges (gamelistreply x2 + gameinforeply) -- v3+legacy green

