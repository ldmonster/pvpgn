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
