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
