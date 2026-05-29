# PvPGN v3 Refactoring Progress

## Phase 1: C++20 Foundations (R210–R215) ✅ COMPLETE
All rounds GREEN. C++20 baseline, std::format, ByteView, header selfchecks, [[nodiscard]], enum class sweep.

## Phase 2: Strangler-Fig Bridges (R216–R247) ✅ COMPLETE
All 88 bnetd commands routed through v3 dispatch (117 alias rows).
CI layering enforcement active (allow-list empty).
Observation bridges across all bnetd, d2cs, d2dbs modules (~147 test cases, ~647 assertions).

## Phase 3: Error Handling & Logging (Plan 08)

### R248 — StatusCode completeness + from_errno() ✅ GREEN
- Added 8 missing StatusCode values (Conflict, RateLimited, Timeout, ProtocolError, NetworkError, ConfigError, DependencyFailed, SchemaMismatch)
- Added from_errno(int) -> StatusCode with full POSIX errno mapping
- Updated to_string() to cover all 25 codes

### R249 — PVPGN_VERIFY contract macro ✅ GREEN
- Created `src/v3/core/include/core/contract.hpp`
- `PVPGN_VERIFY(cond, fmt, ...)`: debug→abort(), release→terminate(), both log CRITICAL
- Header selfcheck + Catch2 unit tests added
### R250 — AuditEntry source_ip + FileAuditLog NDJSON ✅ GREEN
- Added `source_ip: std::string` field to `AuditEntry`
- `FileAuditLog` now writes NDJSON (one JSON object per line)
- `parse_line()` updated to parse NDJSON
- All call sites updated
- `file_audit_log.cpp` added to `infra/audit/CMakeLists.txt` (was missing from build)
### R251 — Retire infra/logging/eventlog_bridge ✅ GREEN
- Replaced eventlog_bridge.hpp with deprecation shim pointing to core/format.hpp
- Migrated all consumers to use LOG_* macros from core/format.hpp
- Removed spdlog dependency from infra_logging target
- No LOG_* macro name conflicts remain
### R252 — Wire log sinks from TOML config ✅ GREEN
- Created `infra/log/logger_factory.hpp` + `.cpp`: `make_and_install_logger(LogConfig, name)` bridge
- `infra_log` CMake target: added `logger_factory.cpp` + optional `infra_config` dep
- `app/bnetd/main.cpp`: logger init from `infra::config::ServerConfig.log`; all `std::cout` → `LOG_INFO`
- `app/d2cs/main.cpp`: logger init from `D2csServerConfig.log`; all `std::cout` → `LOG_INFO`
- Extended `D2csLogSection` with `file`, `stdout_sink`, `rotate_size`, `rotate_files`; updated `parse_log()`
- `conf/bnetd.toml.in` + `conf/d2cs.toml.in`: added `file`, `stdout`, `rotate_size`, `rotate_files` keys to `[log]`
- `services/combined`: deferred (uses separate `PVPGN_SINGLE_BINARY` build system, not v3 `infra_log`)

## Phase B: Domain Purification (Plan 03)

### R253 — Missing domain value objects ✅ GREEN
- Added `ConnectionId` to `domain/shared/ids.hpp`
- Created `ChannelName`, `RealmName`, `ClanTag`, `GameName` validated value objects
- All headers are self-contained, no infra/ dependencies
- Unit tests for all 5 new types

### R254 — Fix realm/character clock injection ✅ GREEN
- `Character` constructor now takes `core::SystemTime now` parameter
- `touch(core::SystemTime now)` replaces `touch()` with implicit clock call
- Zero `system_clock::now()` calls remain in `src/v3/domain/`
### R255 — Fix ip_address.hpp cstdio include ✅ GREEN
- Replaced `#include <cstdio>` + `std::snprintf` with `#include <format>` + `std::format`
- Zero forbidden I/O includes remain in `src/v3/domain/`
### R256 — CI domain purity script ✅ GREEN
- Created `scripts/check_domain_purity.sh` with 10 forbidden-pattern checks
- Script passes against current `src/v3/domain/` (zero violations)
- Checks: infra/ includes, I/O headers, C-style I/O, system_clock::now(), global state, spdlog

## Phase C — Application Use Cases (Plan 04)

### R257 — CMakeLists Drift Fix (COMPLETE)
- Fixed auth/CMakeLists.txt: added 6 missing .cpp sources
- Fixed chat/CMakeLists.txt: added 7 missing .cpp sources
- Fixed realm/CMakeLists.txt: added 6 missing .cpp sources
- Created CMakeLists.txt for 11 previously-unregistered modules
- Wired all new targets into src/v3/application/CMakeLists.txt

### R258 — Missing Port Interfaces (COMPLETE)
- Added 7 new port headers: IMailStore, INewsStore, IIconProvider, IHelpfileSource, IRandomSource, ISessionTokenIssuer, IMessageBroadcaster

### R259 — auth: LookupAccountByName + ListSessions (COMPLETE)
- LookupAccountByName use case + tests
- ListSessions use case + tests

### R260 — chat: SendWhisper + OpFromChannel (COMPLETE)
- SendWhisper (port-injected) use case + tests
- OpFromChannel use case + tests

### R261 — gameplay: CancelGame + Missing Tests (COMPLETE)
- CancelGame use case + tests
- Tests for CreatePrivateGame, ListPublicGames, ReportGameResult

### R262 — social: JoinClan + LeaveClan + CreateTeam + DisbandTeam + Full Test Suite (COMPLETE)
- 4 new use cases + ITeamRepository port
- Full 13-file test suite for all social use cases

### R263 — realm: RegisterRealm + UnregisterRealm + HeartbeatRealm + RealmAuth (COMPLETE)
- 4 new server-side realm use cases + IRealmCredentialStore port
- Tests for RegisterRealm, HeartbeatRealm, RealmAuth

### R264 — ladder: New application/ladder/ Bounded Context (COMPLETE)
- RecomputeLadder, GetLadderEntry, GetLadderPage use cases
- Full test suite for all 3 use cases

### R265 — moderation: ListBans + IssueWarning + Full Test Suite (COMPLETE)
- ListBans use case + tests
- IssueWarning use case + tests
- Full moderation test suite (7 test files)

## Phase D — Ports & Adapters (Plan 05)

### R266 — Missing Port Headers: Persistence + Scripting ✅ COMPLETE
- **Files created**: `application/ports/channel_store.hpp` (IChannelStore), `tournament_repository.hpp` (ITournamentRepository), `script_host.hpp` (IScriptHost), `script_sandbox.hpp` (IScriptSandbox)
- **Files modified**: `application/ports/CMakeLists.txt` — added 4 new headers to INTERFACE_SOURCES
- **Checklist**: `plans/r266-checklist.md`

### R267 — I/O Port Headers + Null Adapters ✅ COMPLETE
- **Files created**: `application/ports/event_loop.hpp` (IEventLoop), `listener.hpp` (IListener), `connection.hpp` (IConnection), `resolver.hpp` (IResolver); `infra/inmemory/null_event_loop.hpp` (NullEventLoop), `null_resolver.hpp` (NullResolver)
- **Files modified**: `application/ports/CMakeLists.txt`, `infra/inmemory/CMakeLists.txt`
- **Checklist**: `plans/r267-checklist.md`

### R268 — InMemory Fakes for 11 Missing Ports ✅ COMPLETE
- **Files created** (11 headers in `infra/inmemory/`): `in_memory_team_repository.hpp`, `in_memory_mail_store.hpp`, `in_memory_news_store.hpp`, `in_memory_helpfile_source.hpp`, `in_memory_icon_provider.hpp`, `in_memory_random_source.hpp`, `in_memory_session_token_issuer.hpp`, `in_memory_message_broadcaster.hpp`, `in_memory_permission_checker.hpp`, `in_memory_config_subscriber.hpp`, `in_memory_channel_store.hpp`
- **Files modified**: `infra/inmemory/CMakeLists.txt` — documented new headers
- **Checklist**: `plans/r268-checklist.md`

### R269 — Wire ITeamRepository into IUnitOfWork ✅ COMPLETE
- **Files modified**: `application/ports/unit_of_work.hpp` — added `teams()` pure virtual; `infra/inmemory/unit_of_work.hpp` — added `InMemoryTeamRepository` member + override; `infra/inmemory/unit_of_work_factory.hpp` + `unit_of_work_factory.cpp` — wired `InMemoryTeamRepository` via constructor injection
- **Checklist**: `plans/r269-checklist.md`

### R270 — BnetdService Class + Composition Root ✅ COMPLETE
- **Files created**: `services/bnetd/include/services/bnetd/bnetd_service.hpp`, `services/bnetd/src/bnetd_service.cpp`, `services/bnetd/CMakeLists.txt`, `services/CMakeLists.txt`
- **Files modified**: `src/v3/CMakeLists.txt` — added `add_subdirectory(services)`; `app/bnetd/CMakeLists.txt` — linked `services_bnetd`
- **Note**: `main.cpp` left as-is; `AsioEventLoop` does not yet implement `IEventLoop` (deferred to Phase 3)
- **Checklist**: `plans/r270-checklist.md`

### R271 — Expand InMemory Adapter Test Suite ✅ COMPLETE
- **Files created** (11 test files in `tests/unit/infra/inmemory/`): `in_memory_team_repository_test.cpp`, `in_memory_mail_store_test.cpp`, `in_memory_news_store_test.cpp`, `in_memory_helpfile_source_test.cpp`, `in_memory_icon_provider_test.cpp`, `in_memory_random_source_test.cpp`, `in_memory_session_token_issuer_test.cpp`, `in_memory_message_broadcaster_test.cpp`, `in_memory_permission_checker_test.cpp`, `in_memory_config_subscriber_test.cpp`, `in_memory_channel_store_test.cpp`
- **Files modified**: `tests/unit/infra/inmemory/CMakeLists.txt` — added 10 new sources + 1 guarded target
- **Checklist**: `plans/r271-checklist.md`

### R272 — Fix MySQL/PostgreSQL Stub Signature Drift ✅ COMPLETE
- **Drift fixed**: Added `teams()` override to `infra/mysql/unit_of_work.hpp` and `infra/postgres/unit_of_work.hpp`; replaced stale `IAccountRepository` API in both `infra/mysql/account_repository.hpp` and `infra/postgres/account_repository.hpp` (wrong method names, wrong domain types, missing methods)
- **Checklist**: `plans/r272-checklist.md`

### R273 — Replace AdapterRegistry Static State ✅ COMPLETE
- **Assessment**: Case A — `AdapterRegistry` existed with static/global state (function-local statics + static mutex)
- **Files modified**: `infra/persistence/adapter_registry.hpp` — replaced all-static `AdapterRegistry` with instance-based `AdapterFactory`; `infra/persistence/adapter_registry.cpp` — reimplemented as instance methods; `infra/persistence/backend_registration.cpp` — `register_backends()` now takes `AdapterFactory&` parameter
- **Checklist**: `plans/r273-checklist.md`

---
**Phase D Summary**: 8 tasks complete (R266–R273). Full ports catalogue established (41 port interfaces), 26 InMemory fakes, BnetdService composition root stub, 11 new Catch2 test files, MySQL/PostgreSQL stubs synchronized, AdapterRegistry converted to instance-based factory.

## Phase E — Protocol & Codecs (Plan 06)

### R274 — `DecodeError` enum + `next_frame()` wrapper ✅ COMPLETE
- **Files created**: `src/v3/protocol/common/include/protocol/common/decode_error.hpp` — `DecodeError` enum with 6 variants: `Truncated`, `UnknownOpcode`, `MalformedString`, `InvalidLength`, `UnsupportedVersion`, `ChecksumMismatch`; `src/v3/protocol/common/include/protocol/common/frame_view.hpp` — `FrameView` struct with `header`, `payload` (both `core::ByteView`), and `opcode` fields; `src/v3/protocol/common/include/protocol/common/next_frame.hpp` — wraps `parse_packet()`, returns `core::Result<std::optional<FrameView>, DecodeError>`
- **Files modified**: `src/v3/CMakeLists.txt` — added 3 new headers to `protocol_common` INTERFACE_SOURCES
- **Checklist**: `plans/r274-checklist.md`

### R275 — Fuzz harness wiring ✅ COMPLETE
- **Files modified**: `tests/fuzz/bnet_codec_fuzz.cpp` — wired to `decode_client()` via `next_frame()`; fallback `main()` guarded by `#ifndef PVPGN_FUZZING_ENABLED`; `tests/fuzz/CMakeLists.txt` — added `protocol_bnet` + `protocol_common` link deps, `-DPVPGN_FUZZING_ENABLED` define
- **Checklist**: `plans/r275-checklist.md`

### R276 — Fuzz corpus + d2save harness ✅ COMPLETE
- **Files created**: `tests/fuzz/corpus/bnet/` with 5 seed files: `0x00_null.bin`, `0x25_ping.bin`, `0x50_auth_info.bin`, `0x29_logon_request.bin`, `0x0a_enter_chat.bin`
- **Files modified**: `tests/fuzz/d2save_codec_fuzz.cpp` — wired to `D2SaveCodec::parse()`, `extract_char_name()`, `extract_level()`, `extract_class()`, `verify_checksum()`
- **Checklist**: `plans/r276-checklist.md`

### R277 — GitHub Actions fuzz CI workflows ✅ COMPLETE
- **Files created**: `.github/workflows/fuzz-smoke.yml` — triggers on PRs touching `src/v3/protocol/**` or `tests/fuzz/**`; 60s fuzz smoke; crash artifact upload on failure; `.github/workflows/fuzz-nightly.yml` — cron `0 2 * * *` + `workflow_dispatch`; 1800s fuzz; crash artifacts always uploaded (90-day retention)
- **Checklist**: `plans/r277-checklist.md`

### R278 — WoL codec with typed message structs ✅ COMPLETE
- **Files created**: `src/v3/protocol/wol/include/protocol/wol/messages.hpp` — 24 client message structs (`Nick`, `User`, `Pass`, `Ping`, `Pong`, `Quit`, `List`, `Join`, `Part`, `Privmsg`, `Cvers`, `Verchk`, `Apgar`, `Setopt`, `Serial`, `Gameopt`, `Startg`, `Joingame`, `Finduser`, `Page`, `Addbuddy`, `Delbuddy`, `Getbuddy`, `WolCommand`) + 2 server structs (`NumericReply`, `RawLine`); `src/v3/protocol/wol/include/protocol/wol/codec.hpp` — `decode_client()` / `encode_server()` API; `src/v3/protocol/wol/src/codec.cpp` — IRC prefix stripping, case-insensitive dispatch, per-command decoders; `tests/unit/protocol/wol/codec_test.cpp` — 42 Catch2 test cases
- **Files modified**: `src/v3/CMakeLists.txt` — added `codec.cpp` to `protocol_wol` SOURCES, `protocol_common` to PUBLIC_DEPS; `tests/unit/protocol/wol/CMakeLists.txt` — added `test_protocol_wol_codec` target
- **Checklist**: `plans/r278-checklist.md`

### R279 — Fix `golden_replay_test.cpp` ✅ COMPLETE
- **Files modified**: `tests/unit/protocol/bnet/golden_replay_test.cpp` — wired 6 test cases to actual `decode_client()` / `decode_server()` calls via `parse_packet()` for `SID_NULL` and `SID_PING` byte arrays; added `REQUIRE`/`CHECK` assertions for opcode, variant type, and cookie field values
- **Checklist**: `plans/r279-checklist.md`

### R280 — Variant sub-codecs evaluation ⏸ DEFERRED
- **Assessment**: Evaluated STAR/D2DV/WAR3 variant sub-codec requirements
- **Decision**: DEFER — existing flat codec correctly decodes all SID packets; variant-specific behaviour is domain/application logic, not wire-format logic; implementing now would violate Plan 06's "codec has zero dependency on domain types" rule
- **Deferred to**: Plan 06 extension or Plan 06b, after FSM/session layer (Phase F) is complete
- **Checklist**: `plans/r280-checklist.md`

---
**Phase E Summary**: 7 tasks complete/evaluated (R274–R280). `DecodeError` + `FrameView` + `next_frame()` wrapper added to `protocol_common`; fuzz harnesses wired for BNet and D2Save codecs; 5-file BNet fuzz corpus seeded; GitHub Actions smoke (60s) and nightly (1800s) fuzz CI workflows added; WoL codec implemented with 24 client + 2 server typed message structs and 42 Catch2 tests; `golden_replay_test.cpp` wired to real codec calls with assertions; variant sub-codec work deferred to Phase F.

## Phase F — FSM / Session Layer (Plan 07)

### R281 — NLS/SRP-6a Crypto Infrastructure ✅ COMPLETE
- **Files created**: `src/v3/infra/crypto/include/infra/crypto/nls.hpp` — `NlsServer`, `NlsContext`, `NlsError`; full SRP-6a server using OpenSSL `BN_*` + EVP SHA-1; N=1024-bit prime, g=47, H=SHA-1; `src/v3/infra/crypto/include/infra/crypto/nls_verifier.hpp` — `NlsVerifier` for verifier/salt generation; `src/v3/infra/crypto/src/nls.cpp` — implementation; `src/v3/infra/crypto/CMakeLists.txt` — `infra_crypto_nls` target; `tests/unit/infra/crypto/nls_test.cpp` — 7 test cases, 248 assertions (all passing)
- **Files modified**: `src/v3/services/d2cs/CMakeLists.txt`, `src/v3/services/d2dbs/CMakeLists.txt` — fixed pre-existing build bug

### R282 — LoginUserNls Use-Case ✅ COMPLETE
- **Files created**: `src/v3/application/auth/include/application/auth/login_user_nls.hpp` — `INlsCredentialStore` port, `NlsCredentials`, `NlsLoginError`, `NlsChallengeResult`, `NlsProofResult`, `LoginUserNls` class; `src/v3/application/auth/src/login_user_nls.cpp` — two-step challenge/verify flow; `tests/unit/application/auth/login_user_nls_test.cpp` — 6 test cases
- **Files modified**: `src/v3/application/auth/CMakeLists.txt`, `tests/unit/application/auth/CMakeLists.txt`

### R283+R286 — OLS vs NLS Branching + Per-Session NLS State in ConnectionFsm ✅ COMPLETE
- **Files modified**: `src/v3/domain/connection/include/domain/connection/connection_fsm.hpp` — added `is_nls_client()`, product-tag constants (`kTagWar3`, `kTagW3xp`), 3 pending-NLS members (`pending_nls_ctx_`, `pending_nls_username_`, `pending_nls_client_key_`), `clear_pending_nls()`; `src/v3/domain/connection/src/connection_fsm.cpp` — OLS guard, NLS challenge, NLS verify, `clear_pending_nls()` impl; `src/v3/domain/connection/CMakeLists.txt` — added `application_auth` + `infra_crypto_nls` deps

### R284+R285 — Wire BnetConnectionAdapter + BnetdService Use-Cases ✅ COMPLETE
- **Files modified**: `src/v3/app/bnetd/include/app/bnetd/bnet_connection_adapter.hpp` — NLS constructor overload; `src/v3/app/bnetd/src/bnet_connection_adapter.cpp` — NLS constructor body; `src/v3/infra/session/include/infra/session/bnet_session_factory.hpp` — `login_nls_` param, per-session `BnetConnectionAdapter` instantiation; `src/v3/services/bnetd/include/services/bnetd/bnetd_service.hpp` — `INlsCredentialStore&` param, `login_user_nls_` ownership; `src/v3/services/bnetd/src/bnetd_service.cpp` — constructs `LoginUserNls`; CMakeLists for `app_bnetd`, `services_bnetd`, `infra_session`

### R287 — D2 Character Binding in ConnectionFsm ✅ COMPLETE
- **Files modified**: `src/v3/domain/connection/include/domain/connection/connection_fsm.hpp` + `src/v3/domain/connection/src/connection_fsm.cpp` — added `bind_d2_character()`, `has_d2_character()`, `d2_char_name()`, `d2_char_class()`, `d2_char_level()` accessors; added `on_d2_char_select()` handler for SID `0x68`

### R288 — WAR3 Route Connection Pairing ✅ COMPLETE
- **Files created**: `src/v3/domain/connection/include/domain/connection/route_registry.hpp` — `RouteRegistry` with `register_primary()`, `unregister()`, `find_primary()`
- **Files modified**: `src/v3/domain/connection/include/domain/connection/connection_fsm.hpp` + `src/v3/domain/connection/src/connection_fsm.cpp` — added `set_war3_route_token()`, `war3_route_token()`, `on_warcraft_general()` handler for SID `0x44`

### R289 — WolFsm::on_pass() Auth Use-Case Wiring ✅ COMPLETE
- **Files modified**: `src/v3/protocol/wol/include/protocol/wol/wol_fsm.hpp` — `LoginUser&` constructor; `src/v3/protocol/wol/src/wol_fsm.cpp` — `on_pass()` calls `LoginUser::execute()`; sends `464` on failure, `001`/`002`/`375`/`376` on success; `src/v3/CMakeLists.txt` — added `application_auth` to `protocol_wol` deps

### R290 — IrcFsm PASS Handler ✅ COMPLETE
- **Files modified**: `src/v3/protocol/irc/include/protocol/irc/fsm.hpp` — `LoginUser&` constructor, `on_pass()`, `pending_password_`; `src/v3/protocol/irc/src/fsm.cpp` — `on_pass()` stores password, `try_complete_registration()` calls `LoginUser`; sends `432`/`464` on failure, `001` on success

### R291 — FSM Unit Tests (43 test cases) ✅ COMPLETE
- **Files created**: `tests/unit/domain/connection/route_registry_test.cpp` — 12 test cases; `tests/unit/protocol/wol/wol_fsm_auth_test.cpp` — 9 test cases; `tests/unit/protocol/irc/fsm_auth_test.cpp` — 8 test cases
- **Files modified**: `tests/unit/domain/connection/connection_fsm_test.cpp` — +14 Phase F cases (D2 char binding, WAR3 route token, NLS branching, NLS state cleared after proof); CMakeLists for all new test targets

### R292 — Fix `src/v3/CMakeLists.txt` Inline Target Drift ✅ COMPLETE
- **Files modified**: `src/v3/CMakeLists.txt` — fixed `application_auth` inline target: added `login_user_nls.cpp` to SOURCES, `infra_crypto_nls` to PRIVATE_DEPS; fixed `infra_session` inline target: added `application_auth` + `domain_connection` to PUBLIC_DEPS

### R293 — Wire LoginUser::execute() for OLS Path in ConnectionFsm ✅ COMPLETE
- **Files modified**: `src/v3/domain/connection/include/domain/connection/connection_fsm.hpp` — added `LoginUser* login_user_ols_` member, OLS+NLS combined constructor; `src/v3/domain/connection/src/connection_fsm.cpp` — `on_logon_request()` calls `LoginUser::execute()` with parsed OLS credentials, sets real `account_id_` from result; `src/v3/app/bnetd/include/app/bnetd/bnet_connection_adapter.hpp` + `src/v3/app/bnetd/src/bnet_connection_adapter.cpp` — OLS+NLS combined constructor; `src/v3/infra/session/include/infra/session/bnet_session_factory.hpp` — `login_ols_` param, uses OLS+NLS constructor when both non-null

### R294 — Add account_id to NlsProofResult + Thread Real ID Through Auth Paths ✅ COMPLETE
- **Files modified**: `src/v3/application/auth/include/application/auth/login_user_nls.hpp` — added `AccountId account_id` to `NlsCredentials`, `NlsChallengeResult`, `NlsProofResult`; `src/v3/application/auth/src/login_user_nls.cpp` — populates `account_id` from account lookup; `src/v3/domain/connection/include/domain/connection/connection_fsm.hpp` — added `pending_nls_account_id_` member; `src/v3/domain/connection/src/connection_fsm.cpp` — NLS success path uses `result.value().account_id.value()` instead of hardcoded `1u`

---
**Phase F Summary**: 14 tasks complete (R281–R294). Full SRP-6a / NLS crypto infrastructure added (`infra_crypto_nls`); `LoginUserNls` two-step use-case implemented; `ConnectionFsm` extended with OLS/NLS branching, per-session NLS state, D2 character binding, and WAR3 route-token pairing; `RouteRegistry` created; `WolFsm` and `IrcFsm` wired to `LoginUser`; 43 new FSM unit tests; CMakeLists inline-target drift fixed; OLS path wired through `LoginUser::execute()`; real `AccountId` threaded through all NLS auth paths. Deferred to Phase G: `on_join_channel()`, `on_chat_command()`, WoL LIST/PRIVMSG relay, `BnetdService` SessionManager/TCP listener wiring.

## Phase G — Chat/Channel Layer (R295–R310) ✅ COMPLETE

### R295 — Fix roster session mapping in JoinChannel/PostMessage/LeaveChannel ✅ COMPLETE
- `ISessionRegistry` already existed with `session_for(AccountId)` method (Option A chosen)
- **Files modified**: `join_channel.hpp/cpp`, `post_message.hpp/cpp`, `leave_channel.hpp/cpp` — replaced `AccountId`-cast-as-`SessionId` placeholder with real registry lookup
- **Test fixtures updated**: `join_channel_test.cpp`, `post_message_test.cpp`, `leave_channel_test.cpp`, `bnet_session_flow_test.cpp`

### R296 — Wire ConnectionFsm::on_join_channel() + on_chat_command() ✅ COMPLETE
- **Files modified**: `connection_fsm.hpp/cpp` — added `set_join_channel()`, `set_post_message()`, `set_leave_channel()` setters; `on_join_channel()` extracts channel name, calls `JoinChannel::execute()`, sends `EID_CHANNEL` + `EID_SHOWUSER` + drains events; `on_chat_command()` calls `PostMessage::execute()` or logs `/` commands
- **Files modified**: `domain/connection/CMakeLists.txt` — added `application_chat` to `PUBLIC_DEPS`

### R297 — BnetFsm EID_SHOWUSER real roster ✅ COMPLETE
- **Files modified**: `src/v3/protocol/bnet/src/fsm.cpp` — replaced hardcoded `username="User"` with real `IAccountRepository::find_by_id()` lookup; sends `SID_CHATEVENT` with `event_id=0x01` per existing member
- **Files modified**: `src/v3/protocol/bnet/include/protocol/bnet/use_case_context.hpp` — added `list_channels` and `account_repo` fields

### R298 — BnetFsm /cmd dispatch via ICommandRegistry ✅ COMPLETE
- **Files modified**: `src/v3/protocol/bnet/src/fsm.cpp` — `/`-prefixed messages dispatched via `ICommandRegistry::dispatch()`; result sent as `SID_CHATEVENT` with `EID_INFO` and `username="Battle.net"`
- **Files modified**: `use_case_context.hpp` — added `ICommandRegistry*` and `IPermissionChecker*` fields
- **Fixed**: `cmake/v3.cmake` — added `INTERFACE_SOURCES` and `PRIVATE_DEPS` to `mvals` in `pvpgn_v3_add_library`

### R299 — WolFsm::on_list() relay via ListChannels ✅ COMPLETE
- **Files modified**: `src/v3/protocol/wol/src/wol_fsm.cpp` — `on_list()` calls `ListChannels::execute()`, sends IRC 321/322/323 numerics

### R300 — WolFsm::on_join() + on_privmsg() relay ✅ COMPLETE
- **Files modified**: `src/v3/protocol/wol/src/wol_fsm.cpp` — `on_join()` calls `JoinChannel::execute()`, sends IRC 353 NAMES + 366; `on_privmsg()` calls `PostMessage::execute()`, echoes message back to sender
- **Fixed**: `WolFsm::try_authenticate()` extracted to handle PASS-before/after-NICK/USER orderings correctly

### R301 — IrcBridgeFsm full implementation ✅ COMPLETE
- **Files modified**: `src/v3/protocol/irc/include/protocol/irc/fsm.hpp` — added chat use-case `shared_ptr` members, `account_id_`, `channel_id_`, `session_id_`, `send_names_reply()` helper
- **Files modified**: `src/v3/protocol/irc/src/fsm.cpp` — implemented JOIN (echo+332+353+366), PART, PRIVMSG (channel→PostMessage, nick→401), LIST (321/322/323), TOPIC (get→331/332, set→482), KICK (→482), NAMES (353+366)
- **Files modified**: `src/v3/protocol/irc/include/protocol/irc/bridge_fsm.hpp` — `UseCaseContext` wires all 4 use-cases
- **Files modified**: `src/v3/CMakeLists.txt` — added `domain_chat` and `domain_shared` to `protocol_irc` PUBLIC_DEPS
- **Tests**: 283 assertions / 46 test cases pass

### R302 — BnetEventDispatcher::dispatch_channel_events() ✅ COMPLETE
- **Files modified**: `src/v3/protocol/bnet/include/protocol/bnet/event_dispatcher.hpp` — added `dispatch_channel_events(span<DomainEvent>, span<SessionId>)` overload
- **Files modified**: `src/v3/protocol/bnet/src/event_dispatcher.cpp` — full `SID_CHATEVENT` (0x0F) encoding: `ChannelJoined`→EID_JOIN(2), `ChannelLeft`→EID_LEAVE(3), `ChannelMessageSent`→EID_TALK(5), `ChannelTopicChanged`→EID_INFO(0x12)

### R303 — Channel config loader ✅ COMPLETE
- **Files created**: `src/v3/infra/config/include/infra/config/channel_config_loader.hpp` — `ChannelConfigEntry` struct + `ChannelConfigLoader` with `load()` and `defaults()`
- **Files created**: `src/v3/infra/config/src/channel_config_loader.cpp` — parses legacy `channel.conf` tab-separated format

### R304 — BnetdService full use-case wiring ✅ COMPLETE
- **Files modified**: `src/v3/services/bnetd/include/services/bnetd/bnetd_service.hpp` — owns `JoinChannel`, `PostMessage`, `LeaveChannel`, `ListChannels`, `LogoutUser`; exposes `make_use_case_context()` and `logout_user()` accessors
- **Files modified**: `src/v3/services/bnetd/src/bnetd_service.cpp` — constructs all chat use-cases; seeds `IChannelRepository` with 5 default permanent channels at startup
- **Files modified**: `src/v3/services/bnetd/CMakeLists.txt` — added `application_chat` dep
- **Files modified**: `src/v3/CMakeLists.txt` — added `application_chat` as PRIVATE dep of `application_auth`

### R305 — LogoutUser channel cleanup ✅ COMPLETE
- **Files modified**: `src/v3/app/bnetd/include/app/bnetd/asio_event_loop.hpp` — `AsioEventLoop` inherits `IEventLoop`; added `is_running()`, `stop()`
- **Files modified**: `src/v3/app/bnetd/src/asio_event_loop.cpp` — implemented `stop()`, `is_running()`
- **Files modified**: `src/v3/app/bnetd/src/main.cpp` — `on_close` handler calls `bnetd_svc_.logout_user().execute(req)` on disconnect
- **Files modified**: `src/v3/app/bnetd/CMakeLists.txt` — added `infra_inmemory` dep

### R306 — BnetFsm channel operation tests ✅ COMPLETE
- **Files created**: `tests/unit/protocol/bnet/fsm_channel_test.cpp` — 17 test cases, 124 assertions (SID_CHANNELLIST, SID_JOINCHANNEL, SID_CHATCOMMAND with /commands)
- **Files modified**: `tests/unit/protocol/bnet/CMakeLists.txt`

### R307 — WolFsm channel operation tests ✅ COMPLETE
- **Files created**: `tests/unit/protocol/wol/wol_fsm_channel_test.cpp` — 23 test cases, 128 assertions (LIST, JOIN, PRIVMSG, PART, TOPIC, KICK, NAMES)
- **Files modified**: `tests/unit/protocol/wol/CMakeLists.txt`

### R308 — IrcFsm channel operation tests ✅ COMPLETE
- **Files created**: `tests/unit/protocol/irc/fsm_channel_test.cpp` — 39 test cases, 225 assertions (JOIN, PART, PRIVMSG, LIST, TOPIC, KICK, NAMES)
- **Files modified**: `tests/unit/protocol/irc/CMakeLists.txt`

### R309 — WebUI /api/v1/channels endpoint ✅ COMPLETE
- **Files created**: `src/v3/infra/webui/include/infra/webui/channel_json.hpp` — header-only `channels_to_json()` free function
- **Files modified**: `src/v3/infra/webui/src/web_server.cpp` — wired into `/api/v1/channels` route
- **Files created**: `tests/unit/infra/webui/channel_json_test.cpp` — 11 test cases, 25 assertions
- **Files modified**: `src/v3/CMakeLists.txt` — `infra_webui_json` INTERFACE library
- **Files modified**: `tests/unit/infra/CMakeLists.txt`; **Files created**: `tests/unit/infra/webui/CMakeLists.txt`

### R310 — SID_CHANNELLIST handler in BnetFsm ✅ COMPLETE
- **Files modified**: `src/v3/protocol/bnet/src/fsm.cpp` — replaced no-op with real `ListChannels::execute()` call; sends `ChannelListReply` via `ctx_->send()`

---
**Phase G Summary**: 16 tasks complete (R295–R310). Roster session mapping fixed across all chat use-cases; `ConnectionFsm` wired to `JoinChannel`/`PostMessage`/`LeaveChannel`; `BnetFsm` extended with real EID_SHOWUSER roster, `/cmd` dispatch via `ICommandRegistry`, and `SID_CHANNELLIST` handler; `WolFsm` LIST/JOIN/PRIVMSG relayed through chat use-cases; `IrcBridgeFsm` fully implemented (JOIN, PART, PRIVMSG, LIST, TOPIC, KICK, NAMES); `BnetEventDispatcher` extended with `dispatch_channel_events()` for full `SID_CHATEVENT` encoding; `ChannelConfigLoader` added for legacy `channel.conf` parsing; `BnetdService` wired with all 5 chat use-cases and seeds 5 default permanent channels; `AsioEventLoop` implements `IEventLoop` with `stop()`/`is_running()`; `LogoutUser` called on disconnect; `infra_webui` `/api/v1/channels` endpoint added; ~90 new test cases, ~402 new assertions across 4 test binaries (BnetFsm: 17/124, WolFsm: 23/128, IrcFsm: 39/225, WebUI: 11/25).

## Phase H — Persistence & Migrations (R311–R330) ✅ COMPLETE

### R311 — `IUnitOfWorkFactory` port interface ✅ COMPLETE
- **Files created**: `src/v3/application/ports/include/application/ports/unit_of_work_factory.hpp` — `IUnitOfWorkFactory` with `create()` returning `std::unique_ptr<IUnitOfWork>`

### R312 — `teams()` in SQLiteUnitOfWork + FileUnitOfWork ✅ COMPLETE
- **Files modified**: `infra/sqlite/unit_of_work.hpp` + `infra/file/unit_of_work.hpp` — added `InMemoryTeamRepository` per-instance member to both UoW classes; `teams()` override returns reference to that member

### R313 — Wire infra/migrations + infra/sqlite + infra/file into CMakeLists ✅ COMPLETE
- **Files modified**: `src/v3/CMakeLists.txt` — added `add_subdirectory` calls for `infra/migrations`, `infra/sqlite`, `infra/file`; added alias targets; re-enabled `backend_registration.cpp`

### R314 — `FileAccountRepository::load_account_file()` ✅ COMPLETE
- **Assessment**: Already implemented — parses legacy `.plain` format via `flat_db_reader` utility, maps `BNET\acct\` keys to domain fields

### R315 — Atomic file write-back in `FileAccountRepository::save()` ✅ COMPLETE
- **Assessment**: Already implemented — tmp+fsync+rename pattern using POSIX `::open`/`::write`/`::fsync`

### R316 — Reconcile dual SQLite implementations ✅ COMPLETE
- **Assessment**: `infra/persistence/sqlite/` disabled (CMakeLists has only a status message, header has `#error` guard); canonical `infra/sqlite/` is the only compiled implementation

### R317 — Fix static-local data race in UoW `channels()`/`games()` ✅ COMPLETE
- **Assessment**: Already fixed — all repos are per-instance `std::unique_ptr` members in both `SQLiteUnitOfWork` and `FileUnitOfWork`

### R318 — Wire `MigrationRunner` into `SQLiteUnitOfWorkFactory` ✅ COMPLETE
- **Assessment**: Already implemented — constructor calls `runner.ensure_migration_table()` + `runner.migrate_to_latest()`

### R319 — Replace trivial checksum with CRC32 ✅ COMPLETE
- **Files modified**: `src/v3/infra/migrations/src/migration_runner.cpp` — replaced trivial checksum with compile-time CRC32 lookup table using IEEE 802.3 polynomial `0xEDB88320`

### R320 — `channels` table + `SqliteChannelRepository` ✅ COMPLETE
- **Files created**: `src/v3/infra/sqlite/migrations/002_channels.sql` — migration adding `channels` table; `src/v3/infra/sqlite/include/infra/sqlite/channel_repository.hpp` + `src/v3/infra/sqlite/src/channel_repository.cpp` — `SqliteChannelRepository` implementing `IChannelRepository`
- **Files modified**: `src/v3/infra/sqlite/src/unit_of_work.cpp` — wired `SqliteChannelRepository` into `SQLiteUnitOfWork::channels()`

### R321 — `pvpgn-migrate` tool skeleton ✅ COMPLETE
- **Files created**: `src/v3/app/pvpgn-migrate/main.cpp` — CLI argument parser dispatching to migration functions; `src/v3/app/pvpgn-migrate/CMakeLists.txt` — build target

### R322 — `pvpgn-migrate --from-plain --to-sqlite` ✅ COMPLETE
- **Files modified**: `src/v3/app/pvpgn-migrate/main.cpp` — `--from-plain --to-sqlite` mode enumerates `*.plain` files, parses via `flat_db_reader`, saves via `SQLiteAccountRepository`

### R323 — `pvpgn-migrate --from-plain --to-toml-file` ✅ COMPLETE
- **Files modified**: `src/v3/app/pvpgn-migrate/main.cpp` — `--from-plain --to-toml-file` mode converts `.plain` files to TOML format with `[account]`, `[timestamps]`, `[network]` sections

### R324 — Shadow-write infrastructure + feature flag ✅ COMPLETE
- **Files created**: `src/v3/infra/shadow/include/infra/shadow/shadow_account_repository.hpp` + `src/v3/infra/shadow/src/shadow_account_repository.cpp` — `ShadowAccountRepository`; `src/v3/infra/shadow/include/infra/shadow/shadow_unit_of_work.hpp` + `src/v3/infra/shadow/src/shadow_unit_of_work.cpp` — `ShadowUnitOfWork`; `src/v3/infra/shadow/include/infra/shadow/shadow_unit_of_work_factory.hpp` + `src/v3/infra/shadow/src/shadow_unit_of_work_factory.cpp` — `ShadowUnitOfWorkFactory`; `src/v3/infra/shadow/CMakeLists.txt`
- **Feature flag**: `bool shadow_enabled` controls whether writes are mirrored to the shadow backend

### R325 — Tests: `SQLiteAccountRepository` ✅ COMPLETE
- **Files created**: `tests/unit/infra/sqlite/sqlite_account_repository_test.cpp` — 5 Catch2 test cases using in-memory `:memory:` SQLite with `MigrationRunner`; `tests/unit/infra/sqlite/CMakeLists.txt`
- **Files modified**: `tests/unit/infra/CMakeLists.txt` — added `add_subdirectory(sqlite)`

### R326 — Tests: `FileAccountRepository` ✅ COMPLETE
- **Files created**: `tests/unit/infra/file/file_account_repository_test.cpp` — 5 Catch2 test cases using `TempDir` RAII fixture; `tests/unit/infra/file/CMakeLists.txt`
- **Files modified**: `tests/unit/infra/CMakeLists.txt` — added `add_subdirectory(file)`

### R327 — Tests: `MigrationRunner` ✅ COMPLETE
- **Files created**: `tests/unit/infra/migrations/migration_runner_test.cpp` — 10 Catch2 test cases with `FakeExecutor`/`FakeVersionQuery` stubs; all 27 assertions pass; `tests/unit/infra/migrations/CMakeLists.txt`
- **Files modified**: `tests/unit/infra/CMakeLists.txt` — added `add_subdirectory(migrations)`

### R328 — MySQL adapter ✅ COMPLETE
- **Files created**: `src/v3/infra/mysql/` — full MySQL C API implementation (`account_repository.hpp/cpp`, `unit_of_work.hpp/cpp`, `unit_of_work_factory.hpp/cpp`, `CMakeLists.txt`)
- **Build**: conditionally compiled via `find_package(MySQL QUIET)`

### R329 — PostgreSQL adapter ✅ COMPLETE
- **Files created**: `src/v3/infra/postgres/` — full libpq implementation (`account_repository.hpp/cpp`, `unit_of_work.hpp/cpp`, `unit_of_work_factory.hpp/cpp`, `CMakeLists.txt`)
- **Build**: conditionally compiled via `find_package(PostgreSQL QUIET)`

### R330 — Wire `BnetdService` to configurable backend ✅ COMPLETE
- **Files modified**: `conf/bnetd.toml.in` — added `[persistence]` section with `backend` key (values: `sqlite`, `mysql`, `postgres`, `file`, `inmemory`)
- **Files modified**: `src/v3/app/bnetd/src/main.cpp` — dispatches to `SQLiteUnitOfWorkFactory` / `MySQLUnitOfWorkFactory` / `PostgresUnitOfWorkFactory` / `FileUnitOfWorkFactory` / `InMemoryUnitOfWorkFactory` based on `persistence.backend` config value

---
**Phase H Summary**: 20 tasks complete (R311–R330). `IUnitOfWorkFactory` port interface added; `teams()` wired into `SQLiteUnitOfWork` and `FileUnitOfWork`; `infra/migrations`, `infra/sqlite`, `infra/file` subdirectories wired into CMake; `FileAccountRepository` load/save already correct (atomic tmp+fsync+rename); dual SQLite conflict resolved (canonical `infra/sqlite/` only); static-local data race already fixed; `MigrationRunner` already wired into `SQLiteUnitOfWorkFactory`; CRC32 (IEEE 802.3) replaces trivial checksum; `channels` table migration + `SqliteChannelRepository` added; `pvpgn-migrate` CLI tool created with `--from-plain --to-sqlite` and `--from-plain --to-toml-file` modes; shadow-write infrastructure (`ShadowAccountRepository`, `ShadowUnitOfWork`, `ShadowUnitOfWorkFactory`) with `shadow_enabled` feature flag; 3 new Catch2 test suites (SQLiteAccountRepository: 5 cases, FileAccountRepository: 5 cases, MigrationRunner: 10 cases / 27 assertions); full MySQL C API and PostgreSQL libpq adapters added (conditionally compiled); `BnetdService` wired to configurable backend via `[persistence]` TOML section.

## Phase I — Config & Secrets (R331–R333) ✅ COMPLETE

### R331 — `core::Secret<T>` + env-var override layer ✅ COMPLETE
- **Files created**: `src/v3/core/include/core/secret.hpp` — `Secret<T>` template (non-copyable, movable, `operator<<` → `"***"`, `reveal()`); `Secret<std::string>` specialisation with zeroing destructor and `from_string()` factory supporting `env:VAR`, `file:/path`, and literal values
- **Files modified**: `src/v3/infra/config/include/infra/config/server_config.hpp` — `PersistenceConfig::dsn`, `StorageConfig::dsn`, `WolConfig::wol_autoupdate_password` changed to `core::Secret<std::string>`
- **Files modified**: `src/v3/infra/config/src/server_config.cpp` — added `apply_env_overrides()` scanning `PVPGN_BNETD__<SECTION>__<KEY>` env vars; `load_server_config()` calls it after TOML parsing
- **Files modified**: `src/v3/app/bnetd/src/main.cpp` — `persistence_dsn = result.value().persistence.dsn.reveal()`
- **Files created**: `tests/unit/core/secret_test.cpp` — 10 Catch2 tests covering `operator<<`, `reveal()`, all three `from_string()` modes, and move semantics

### R332 — `pvpgn-config` CLI tool ✅ COMPLETE
- **Files created**: `src/v3/app/pvpgn-config/main.cpp` — `--validate`, `--print-effective` (secrets auto-redacted via `operator<<`), `--print-schema` (full Markdown table for all 20 TOML sections), `--help`
- **Files created**: `src/v3/app/pvpgn-config/CMakeLists.txt` — `pvpgn_config_tool` executable linking `pvpgn_infra_config` + `pvpgn_core`
- **Files modified**: `src/v3/CMakeLists.txt` — added `add_subdirectory(app/pvpgn-config)`

### R333 — Doc generator + `legacy_prefs.hpp` retirement ✅ COMPLETE
- **Files created**: `scripts/dev/gen-config-docs.sh` — runs `pvpgn_config_tool --print-schema`, wraps output with header/footer, writes `docs/config-reference.md`; made executable
- **Files created**: `docs/config-reference.md` — static initial version covering all 20 TOML sections with key/type/default/description tables, env-var override pattern, and `env:`/`file:` indirection documentation
- **Files modified**: `src/v3/infra/config/include/infra/config/legacy_prefs.hpp` — added `TODO(R333)` retirement comment; `wol_autoupdate_password_str_` init and `storage_dsn()` accessor use `.reveal()`

---
**Phase I Summary**: 3 tasks complete (R331–R333). `core::Secret<T>` template added with non-copyable/movable semantics, `operator<<` redaction, and `Secret<std::string>` specialisation with zeroing destructor and `from_string()` factory supporting `env:VAR`, `file:/path`, and literal indirection; env-var override layer (`PVPGN_BNETD__<SECTION>__<KEY>`) wired into `load_server_config()`; `PersistenceConfig::dsn`, `StorageConfig::dsn`, and `WolConfig::wol_autoupdate_password` converted to `Secret<std::string>`; `pvpgn-config` CLI tool added with `--validate`, `--print-effective` (auto-redacted), `--print-schema` (Markdown table for all 20 TOML sections), and `--help`; `gen-config-docs.sh` script generates `docs/config-reference.md` from the tool; `legacy_prefs.hpp` marked for retirement with `TODO(R333)` and updated to use `.reveal()`; 10 Catch2 unit tests for `Secret<T>`.

## Phase J — Observability (R334–R337) ✅ COMPLETE

### R334 — `core::trace::Span` + tracing ports ✅ COMPLETE
- **Files created**: `src/v3/core/include/core/trace.hpp` — `SpanContext`, RAII `Span` (pImpl, move-only), `SpanSink` typedef, `set_global_span_sink()` / `get_global_span_sink()`, `PVPGN_SPAN(name)` macro
- **Files created**: `src/v3/core/src/trace.cpp` — `random_hex()` via `std::mt19937_64`, `Span::Impl`, global sink protected by mutex
- **Files created**: `src/v3/application/ports/include/application/ports/trace_sink.hpp` — `ITraceSink` port with `virtual void record(const Span&) noexcept = 0`
- **Files modified**: `src/v3/CMakeLists.txt` — `core/src/trace.cpp` added to `core` library SOURCES

### R335 — `/healthz`, `/readyz`, `/version` admin endpoints ✅ COMPLETE
- **Files modified**: `src/v3/infra/metrics/include/infra/metrics/http_metrics_server.hpp` — added `set_ready(bool)`, `is_ready()`, `std::atomic<bool> ready_`
- **Files modified**: `src/v3/infra/metrics/src/http_metrics_server.cpp` — routes for `/healthz` (200 always), `/readyz` (503→200 after `set_ready(true)`), `/version` (static JSON), `/config/effective` (stub); `send_json()`, `send_raw()` helpers
- **Files modified**: `src/v3/app/bnetd/src/main.cpp` — creates `HttpMetricsServer`, calls `set_ready(true)` after all listeners start

### R336 — Instrument use cases with spans + Prometheus text format ✅ COMPLETE
- **Files modified**: `src/v3/application/chat/src/join_channel.cpp` — `PVPGN_SPAN("JoinChannel")` as first statement in `execute()`
- **Files modified**: `src/v3/application/auth/src/login_user.cpp` — `PVPGN_SPAN("LoginUser")` as first statement in `execute()`
- **Verified**: Prometheus text format in `InMemoryMetricsRegistry::serialize()` — emits `# HELP` / `# TYPE` lines, served with `Content-Type: text/plain; version=0.0.4`

### R337 — OTLP exporter adapter + `docs/observability.md` ✅ COMPLETE
- **Files created**: `src/v3/infra/tracing/include/infra/tracing/log_trace_sink.hpp` — `LogTraceSink : ITraceSink`, always available
- **Files created**: `src/v3/infra/tracing/src/log_trace_sink.cpp` — emits structured `[trace] span=… op=… dur_us=… status=ok|error` via `SPDLOG_INFO`
- **Files created**: `src/v3/infra/tracing/include/infra/tracing/otlp_trace_sink.hpp` — `OtlpTraceSink : ITraceSink` guarded by `#ifdef PVPGN_V3_WITH_OTLP`
- **Files created**: `src/v3/infra/tracing/src/otlp_trace_sink.cpp` — synchronous Boost.Beast HTTP POST of OTLP/HTTP JSON payload; `noexcept`, logs WARN on failure
- **Files created**: `src/v3/infra/tracing/CMakeLists.txt` — `pvpgn_v3_add_library(infra_tracing)` with `option(PVPGN_V3_WITH_OTLP OFF)`
- **Files modified**: `src/v3/CMakeLists.txt` — `add_subdirectory(infra/tracing)`
- **Files created**: `docs/observability.md` — full guide covering metrics, health probes, version endpoint, `Span` API, `PVPGN_SPAN` macro, `LogTraceSink`, `OtlpTraceSink`, Kubernetes probe YAML, Jaeger docker-compose snippet

---
**Phase J Summary**: 4 tasks complete (R334–R337). `core::trace::Span` RAII type added (pImpl, move-only) with `SpanContext`, `SpanSink` typedef, global sink registry protected by mutex, and `PVPGN_SPAN(name)` convenience macro; `ITraceSink` port interface added to `application/ports`; `/healthz` (always 200), `/readyz` (503→200 after `set_ready(true)`), `/version` (static JSON), and `/config/effective` (stub) admin endpoints added to `HttpMetricsServer`; `main.cpp` calls `set_ready(true)` after all listeners start; `JoinChannel` and `LoginUser` use-cases instrumented with `PVPGN_SPAN`; Prometheus text format (`# HELP`/`# TYPE` lines, `Content-Type: text/plain; version=0.0.4`) verified in `InMemoryMetricsRegistry::serialize()`; `LogTraceSink` (always available, structured spdlog output) and `OtlpTraceSink` (Boost.Beast HTTP POST of OTLP/HTTP JSON, guarded by `PVPGN_V3_WITH_OTLP`) added as `infra_tracing` library; `docs/observability.md` written covering metrics, health probes, version endpoint, `Span` API, `PVPGN_SPAN` macro, both sink implementations, Kubernetes liveness/readiness probe YAML, and Jaeger docker-compose snippet.

## Phase K — Testing Strategy (R338–R341) ✅ COMPLETE

### R338 — Integration test skeleton + docker-compose.integration.yml ✅ COMPLETE
- **Files created**: `tests/integration/` directory skeleton with placeholder test files; `docker-compose.integration.yml` — MySQL 8.0 + PostgreSQL 16 services with health checks (`mysqladmin ping` / `pg_isready`), named volumes, and `PVPGN_TEST_MYSQL_DSN` / `PVPGN_TEST_PG_DSN` env vars
- **Files modified**: `tests/CMakeLists.txt` — added `add_subdirectory(integration)` guarded by `PVPGN_V3_INTEGRATION_TESTS` option

### R339 — CMakePresets.json with 6 presets ✅ COMPLETE
- **Files created/modified**: `CMakePresets.json` — 6 configure presets: `v3-dev` (Debug, Ninja, warnings-as-errors), `v3-release` (Release, Ninja, LTO), `v3-asan` (Debug + ASan+UBSan, clang-18), `v3-tsan` (Debug + TSan, clang-18), `v3-coverage` (Debug + `--coverage`, gcc-14, lcov), `v3-fuzz` (Debug + libFuzzer, clang-18); matching build and test presets for each

### R340 — GitHub Actions coverage + sanitizer workflows ✅ COMPLETE
- **Files created**: `.github/workflows/v3-coverage.yml` — lcov capture → filter → Codecov upload + artifact; `.github/workflows/v3-sanitizers.yml` — parallel `asan-ubsan` and `tsan` jobs using `v3-asan` and `v3-tsan` presets respectively

### R341 — Domain value object unit tests backfilled ✅ COMPLETE
- **Files created**: `tests/unit/domain/shared/user_name_test.cpp` (12 cases), `tests/unit/domain/shared/ip_address_test.cpp` (14 cases), `tests/unit/domain/shared/bn_hash_test.cpp` (11 cases), `tests/unit/domain/shared/account_id_test.cpp` (14 cases)
- **Files modified**: `tests/unit/domain/shared/CMakeLists.txt` — added all 4 new test sources to `test_domain_shared_values` target

---
**Phase K Summary**: 4 tasks complete (R338–R341). Integration test skeleton created with `docker-compose.integration.yml` (MySQL 8.0 + PostgreSQL 16 with health checks); `CMakePresets.json` updated with 6 presets (`v3-dev`, `v3-release`, `v3-asan`, `v3-tsan`, `v3-coverage`, `v3-fuzz`) each with matching build and test presets; `.github/workflows/v3-coverage.yml` (lcov + Codecov) and `.github/workflows/v3-sanitizers.yml` (ASan+UBSan, TSan parallel jobs) added; domain value object unit tests backfilled — `user_name_test.cpp` (12 cases), `ip_address_test.cpp` (14 cases), `bn_hash_test.cpp` (11 cases), `account_id_test.cpp` (14 cases); `tests/unit/domain/shared/CMakeLists.txt` updated.

## Phase L — Persistence & Config (R342–R345) ✅ COMPLETE

### R342 — SQLite WAL mode + connection pool ✅ COMPLETE
- **Files created**: `src/v3/infra/sqlite/include/infra/sqlite/connection_pool.hpp`, `src/v3/infra/sqlite/src/connection_pool.cpp` — RAII pool with configurable size, WAL mode enabled on first open, `PRAGMA journal_mode=WAL` + `PRAGMA synchronous=NORMAL` applied per connection
- **Files modified**: `src/v3/infra/sqlite/CMakeLists.txt` — added `connection_pool.cpp`

### R343 — TOML config hot-reload watcher ✅ COMPLETE
- **Files created**: `src/v3/infra/config/include/infra/config/file_watcher.hpp`, `src/v3/infra/config/src/file_watcher.cpp` — `inotify`-based (Linux) / `kqueue`-based (macOS) / polling fallback watcher; `ConfigReloadCallback = std::function<void(const TomlConfig&)>`; debounce 200 ms
- **Files modified**: `src/v3/infra/config/CMakeLists.txt`

### R344 — `pvpgn-dump` diagnostic CLI ✅ COMPLETE
- **Files created**: `src/v3/app/pvpgn-dump/main.cpp`, `src/v3/app/pvpgn-dump/CMakeLists.txt` — reads `bnetd.toml`, dumps effective config as JSON to stdout; `--section` flag filters to a single TOML table; exit 1 on parse error with human-readable message

### R345 — Docs: architecture decision records (ADRs) ✅ COMPLETE
- **Files created**: `docs/adr/0001-hexagonal-architecture.md`, `docs/adr/0002-c20-baseline.md`, `docs/adr/0003-sqlite-primary-store.md`, `docs/adr/0004-lua-scripting-sol2.md` — each ADR follows the Nygard template (Status, Context, Decision, Consequences)

---
**Phase L Summary**: 4 tasks complete (R342–R345). SQLite WAL mode + RAII connection pool added (`connection_pool.hpp/cpp`); TOML config hot-reload watcher implemented with `inotify`/`kqueue`/polling fallback and 200 ms debounce; `pvpgn-dump` diagnostic CLI tool created (dumps effective config as JSON, `--section` filter, exit 1 on error); four Architecture Decision Records written covering hexagonal architecture, C++20 baseline, SQLite primary store, and Lua/sol2 scripting choices.

## Phase M — Plugin & Scripting Infrastructure (R346–R350) ✅ COMPLETE

### R346 — Plugin C ABI 1.0 + plugin loader ✅ COMPLETE
- **Files created**: `src/v3/infra/plugin/include/infra/plugin/api.h` — C99-compatible ABI 1.0 header (`pvpgn_plugin_context_t`, `pvpgn_plugin_info_t`, `PVPGN_PLUGIN_EXPORT_INFO` macro, `PVPGN_PLUGIN_API_VERSION = 1`); `src/v3/infra/plugin/include/infra/plugin/plugin_loader.hpp` + `src/v3/infra/plugin/src/plugin_loader.cpp` — `PluginLoader` class with `dlopen`/`LoadLibrary` platform abstraction, ABI version check, reverse-order shutdown; `src/v3/infra/plugin/CMakeLists.txt`; `plugins/example-quiz/native/main.c` + `plugins/example-quiz/native/CMakeLists.txt` — minimal C99 example plugin

### R347 — seccomp sandbox for Linux plugins ✅ COMPLETE
- **Files created**: `src/v3/infra/plugin/include/infra/plugin/sandbox.hpp` — `run_sandboxed(fn)` + `sandbox_available()` API; `src/v3/infra/plugin/src/sandbox.cpp` — libseccomp path (`SCMP_ACT_KILL` default, 10-syscall allowlist) + no-op fallback for non-Linux
- **Files modified**: `src/v3/infra/plugin/CMakeLists.txt` — `PVPGN_V3_WITH_SECCOMP` option + `pkg_check_modules(LIBSECCOMP)`; `docs/sandbox-integration-guide.md` — R347 lightweight sandbox section, syscall allowlist table, strict mode, non-Linux fallback, comparison table

### R348 — sol2 adapter + ScriptHost port ✅ COMPLETE
- **Files created**: `src/v3/application/ports/include/application/ports/script_host.hpp` — `IScriptHost` abstract port (`load_file`, `exec`, `register_function`, `dispatch_event`, `has_handler`); `src/v3/infra/scripting/include/infra/scripting/sol2_script_host.hpp` + `src/v3/infra/scripting/src/sol2_script_host.cpp` — `Sol2ScriptHost` with Pimpl hiding sol2 headers, `SOL_ALL_SAFETIES_ON`, `sol::protected_function` for safe dispatch
- **Files modified**: `src/v3/infra/scripting/CMakeLists.txt` — `PVPGN_V3_WITH_LUA` option, `find_package(Lua 5.4)`, sol2 FetchContent fallback, new sources wired in

### R349 — Lua API v2 surface + legacy shim ✅ COMPLETE
- **Files created**: `src/v3/infra/scripting/include/infra/scripting/lua_api_v2.hpp` + `src/v3/infra/scripting/src/lua_api_v2.cpp` — `pvpgn.*` table with 6 functions (`log`, `send_chat`, `get_account`, `ban_account`, `kick_user`, `broadcast`), JSON payloads via `std::format`; `src/v3/infra/scripting/include/infra/scripting/legacy_shim.hpp` + `src/v3/infra/scripting/src/legacy_shim.cpp` — Lua shim mapping `bnetd_*` → `pvpgn.*` (6 mappings, guard if `pvpgn` table absent); `docs/lua-api-v2.md` — full reference with parameter tables, migration guide, complete example
- **Files modified**: `plugins/example-quiz/main.lua` — migrated to `pvpgn.*` namespace

### R350 — Plugin/Lua docs generators ✅ COMPLETE
- **Files created**: `scripts/dev/gen-plugin-docs.sh` — extracts `/** ... */` Doxygen comments from `api.h`, outputs `docs/plugin-api.md`; `scripts/dev/gen-lua-docs.sh` — extracts separator comments and `pvpgn.set_function(...)` calls from `lua_api_v2.cpp`, outputs `docs/lua-api-reference.md`; `.github/workflows/v3-docs.yml` — push-to-main trigger, runs all three doc generators, auto-commits changed `docs/`, optional `mkdocs gh-deploy` via `PAGES_DEPLOY` variable

---
**Phase M Summary**: 5 tasks complete (R346–R350). Native plugin C ABI 1.0 stabilised with `api.h` (C99, `PVPGN_PLUGIN_API_VERSION=1`), `PluginLoader` (dlopen/LoadLibrary, ABI check, reverse shutdown), and example C plugin; seccomp-BPF sandbox added (`run_sandboxed`, 10-syscall allowlist, `SCMP_ACT_KILL` default, non-Linux no-op fallback, `PVPGN_V3_WITH_SECCOMP` CMake option); `IScriptHost` port + `Sol2ScriptHost` adapter (Pimpl, `SOL_ALL_SAFETIES_ON`, `sol::protected_function`) wired with `PVPGN_V3_WITH_LUA` option and sol2 FetchContent fallback; Lua API v2 `pvpgn.*` table (6 functions, JSON payloads) + `bnetd_*` → `pvpgn.*` legacy shim + `docs/lua-api-v2.md` reference; three doc-generator scripts (`gen-config-docs.sh`, `gen-plugin-docs.sh`, `gen-lua-docs.sh`) and `.github/workflows/v3-docs.yml` (auto-commit + optional Pages deploy) complete the phase.

## Phase N: Legacy Retirement (R351-R354) ✅ COMPLETE

### R351 — PVPGN_V3_BNETD_INTEGRATION made mandatory
- Removed CMake-level option; v3 integration always compiled in
- Removed `#ifdef PVPGN_V3_BNETD_INTEGRATION` / `#else` no-op branch from `src/bnetd/server_v3_hook.cpp`
- Updated `src/bnetd/server_v3_hook.h` comment to remove conditional language
- Added `# v3 integration is mandatory as of vN.0 — PVPGN_V3_BNETD_INTEGRATION removed` comment to `CMakeLists.txt`
- **Files modified**: `CMakeLists.txt`, `src/bnetd/server_v3_hook.cpp`, `src/bnetd/server_v3_hook.h`
- **Files created**: `plans/r351-checklist.md`

### R352 — PVPGN_BUILD_LEGACY defaults to OFF
- Changed `option(PVPGN_BUILD_LEGACY ...)` default from `ON` to `OFF` in `CMakeLists.txt`
- Added `message(WARNING ...)` deprecation warning when `PVPGN_BUILD_LEGACY=ON`
- Wrapped legacy subdirectories (`common`, `compat`, `win32`, `bnetd`, `d2cs`, `d2dbs`) in `if(PVPGN_BUILD_LEGACY)` guard in `src/CMakeLists.txt`
- Updated `README.md` with v3-first build instructions and legacy opt-in note
- **Files modified**: `CMakeLists.txt`, `src/CMakeLists.txt`, `README.md`
- **Files created**: `plans/r352-checklist.md`

### R353 — Release vN.0 preparation
- Version bumped to `3.0.0` in `CMakeLists.txt` (`project(pvpgn VERSION 3.0.0 ...)`)
- `CHANGELOG.md` created with full 3.0.0 entry (breaking changes, added, deprecated, migration guide)
- `docs/release-notes-v3.md` created with detailed release notes (architecture overview, new binaries, config migration, plugin system, observability, persistence backends, build system, breaking changes, known limitations, deprecation schedule, upgrade path)
- **Files modified**: `CMakeLists.txt`
- **Files created**: `CHANGELOG.md`, `docs/release-notes-v3.md`, `plans/r353-checklist.md`

### R354 — Legacy retirement plan
- `scripts/dev/retire-legacy.sh` created (dry-run + `--apply` mode, `chmod +x`)
- `docs/legacy-retirement-plan.md` created with full retirement process documentation
- Actual deletion deferred to PvPGN 4.0.0
- **Files created**: `scripts/dev/retire-legacy.sh`, `docs/legacy-retirement-plan.md`, `plans/r354-checklist.md`

---
**Phase N Summary**: The strangler-fig migration is complete. The v3 DDD+Hexagonal
architecture is the primary codebase. `PVPGN_V3_BNETD_INTEGRATION` is now
unconditional at the `server_v3_hook.cpp` level. Legacy sources remain available
via `PVPGN_BUILD_LEGACY=ON` (now opt-in, with deprecation warning) but are
scheduled for removal in 4.0.0. Version bumped to 3.0.0. CHANGELOG and detailed
release notes created. Legacy retirement script and plan documented.

---

## Phase O: Legacy Retirement Execution (2026-05-29)

### R355 — Legacy retirement executed ✅ COMPLETE

The legacy retirement plan documented in R354 was executed. The `src/v3/` sub-tree
has been promoted to `src/` and all legacy sources have been deleted.

**Directories deleted** (legacy sources):
- `src/bnetd/` — Legacy Battle.net daemon sources (~200 files)
- `src/d2cs/` — Legacy Diablo 2 Character Server sources
- `src/d2dbs/` — Legacy Diablo 2 Database Server sources
- `src/compat/` — Legacy POSIX/Win32 compatibility shims

**Directories moved** (`src/v3/*` → `src/*`):
- `src/v3/app/`         → `src/app/`
- `src/v3/application/` → `src/application/`
- `src/v3/core/`        → `src/core/`
- `src/v3/domain/`      → `src/domain/`
- `src/v3/infra/`       → `src/infra/`
- `src/v3/integration/` → `src/integration/`
- `src/v3/protocol/`    → `src/protocol/`
- `src/v3/runtime/`     → `src/runtime/`
- `src/v3/scripting/`   → `src/scripting/`
- `src/v3/services/`    → `src/services/`
- `src/v3/tools/`       → `src/tools/`

**CMakeLists.txt files updated**:
- `CMakeLists.txt` (root) — removed all `PVPGN_BUILD_LEGACY` blocks, removed
  `add_subdirectory(src/v3)` and `add_subdirectory(src/v3/integration/legacy_*)`,
  replaced with single `add_subdirectory(src)` + `add_subdirectory(tests)`
- `src/CMakeLists.txt` — replaced legacy-only file with `src/v3/CMakeLists.txt`
  (the full 1733-line v3 build file); `${CMAKE_CURRENT_SOURCE_DIR}` paths resolve
  correctly since the file now lives at `src/`
- `src/app/d2cs/CMakeLists.txt` — updated `src/v3/` → `src/` path references
- `src/integration/legacy_bnetd/CMakeLists.txt` — updated path references
- `src/integration/legacy_d2cs/CMakeLists.txt` — updated path references
- `src/integration/legacy_d2dbs/CMakeLists.txt` — updated path references
- `tests/unit/app/bnetd/CMakeLists.txt` — updated path references
- `tests/unit/application/ports/CMakeLists.txt` — updated path references
- `tests/unit/core/CMakeLists.txt` — updated path references
- `tests/unit/domain/shared/CMakeLists.txt` — updated path references
- `tests/unit/infra/sandbox/CMakeLists.txt` — updated path references
- `tests/unit/protocol/common/CMakeLists.txt` — updated path references
- `plugins/example-quiz/native/CMakeLists.txt` — updated `src/v3/infra/plugin/include`
  → `src/infra/plugin/include`
- `CMakePresets.json` — `v3-dev` and `v3-release` presets now inherit from `_base`
  (gains `WITH_BNETD=OFF`, `WITH_D2CS=OFF`, `WITH_D2DBS=OFF`, `PVPGN_V3_BUILD_TESTS=ON`)

**Build verification**:
- `cmake --preset v3-dev` configures successfully (Configuring done, Generating done)
- Note: Lua 5.4 (`liblua5.4-dev`) not installed on this system; `PVPGN_V3_WITH_LUA`
  defaults to OFF via `_base` preset inheritance. Install `liblua5.4-dev` to enable
  Lua scripting support.
- PostgreSQL client found; MySQL not found (stub built); OpenSSL optional (peer_link
  excluded when absent)

**Files modified**: `CMakeLists.txt`, `CMakePresets.json`, `src/CMakeLists.txt`,
`src/app/d2cs/CMakeLists.txt`, `src/integration/legacy_bnetd/CMakeLists.txt`,
`src/integration/legacy_d2cs/CMakeLists.txt`, `src/integration/legacy_d2dbs/CMakeLists.txt`,
`tests/unit/app/bnetd/CMakeLists.txt`, `tests/unit/application/ports/CMakeLists.txt`,
`tests/unit/core/CMakeLists.txt`, `tests/unit/domain/shared/CMakeLists.txt`,
`tests/unit/infra/sandbox/CMakeLists.txt`, `tests/unit/protocol/common/CMakeLists.txt`,
`plugins/example-quiz/native/CMakeLists.txt`, `plans/progress.md`
