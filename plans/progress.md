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
