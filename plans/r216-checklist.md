# R216 -- Strangler-fig start for legacy bnetd `handle_command`

Status: **GREEN** (`pvpgn-v3-test:r216`)

## Scope

User-selected: "scaffold + migrate `/help`, `/version`, `/uptime`".

R216 is the first round of the multi-round strangler-fig migration of
`src/bnetd/command.cpp` (~6000 LOC, ~100 chat commands) into the v3
pipeline.

## Architecture

```
legacy handle_command(c, text)
        |
        v
  PVPGN_V3_BNETD_INTEGRATION
        |  yes
        v
  pvpgn_v3_command_dispatch_try(c, text)     <- new bridge symbol
        |                                        in _linked lib
        v
  application::admin_commands::route(text, predicate)   <- PURE v3
        |
        +-- NotFound  -> return 0 -> legacy dispatch loop runs
        +-- Denied    -> send "reserved" / "deactivated" -> return 1
        +-- Handled   -> run_handler(c, text, canonical)
                              |
                              +-- /version: in-bridge string send
                              +-- /uptime : in-bridge string send
                              +-- /help   : delegates to legacy
                                            handle_help_command()
                         -> userlog_append -> return 1
```

The router decision (alias resolution, permission predicate hand-off)
is **pure v3** and lives in `application_admin_commands`. The bridge
(legacy I/O, version string formatting, `seconds_to_timestr` call)
lives in `integration_legacy_bnetd_linked`. The `/help` body still
sits in legacy `helpfile.cpp` -- this is a deliberate first-step
strangle (v3 owns the dispatch, legacy still owns the body).

## Files added

- [src/v3/application/admin_commands/include/application/admin_commands/router.hpp](src/v3/application/admin_commands/include/application/admin_commands/router.hpp)
- [src/v3/application/admin_commands/src/router.cpp](src/v3/application/admin_commands/src/router.cpp)
- [tests/unit/application/admin_commands/CMakeLists.txt](tests/unit/application/admin_commands/CMakeLists.txt)
- [tests/unit/application/admin_commands/router_test.cpp](tests/unit/application/admin_commands/router_test.cpp)
- `plans/r216-checklist.md` (this file)

## Files modified

- [src/v3/CMakeLists.txt](src/v3/CMakeLists.txt) -- new
  `application_admin_commands` static lib; removed
  `command_dispatch_bridge.cpp` from base `integration_legacy_bnetd`
  SOURCES (it now needs legacy headers); added
  `application_admin_commands` to base PUBLIC_DEPS so the router
  header propagates to the linked variant.
- [src/v3/integration/legacy_bnetd/CMakeLists.txt](src/v3/integration/legacy_bnetd/CMakeLists.txt) --
  `command_dispatch_bridge.cpp` is now a `_linked` source.
- [src/v3/integration/legacy_bnetd/src/command_dispatch_bridge.cpp](src/v3/integration/legacy_bnetd/src/command_dispatch_bridge.cpp) --
  rewritten from observer-only into a real dispatcher (router call,
  permission predicate adapter on top of legacy
  `command_get_group` + `account_get_command_groups`, handlers for
  the three migrated commands, denial messages, userlog append).
- [src/bnetd/command.cpp](src/bnetd/command.cpp) -- the legacy
  `handle_command` gate now uses the bridge's return value to
  short-circuit instead of discarding it with `(void)`.
- [tests/unit/application/CMakeLists.txt](tests/unit/application/CMakeLists.txt) --
  added gated `add_subdirectory(admin_commands)`.
- [tests/unit/integration/legacy_bnetd/CMakeLists.txt](tests/unit/integration/legacy_bnetd/CMakeLists.txt) --
  removed `test_integration_legacy_bnetd_command_dispatch_bridge`.
  The old observer-only test no longer applies; pure-v3 routing is
  covered by the new admin_commands router test.
- [Dockerfile.v3](Dockerfile.v3) -- swapped the obsolete bridge
  test for `test_application_admin_commands_router` in both the
  build-target list and the run-step list.

## Files removed

- `tests/unit/integration/legacy_bnetd/command_dispatch_bridge_test.cpp` --
  tested the old observer-only contract; the new bridge requires a
  real legacy `t_connection*` (calls `conn_get_account` /
  `message_send_text` etc.) and can no longer be exercised with a
  fake int marker.

## Caveats documented in source

1. `/uptime` response is English-only on the v3 path. Legacy used
   `localize(c, "Uptime: {}", ...)`. The v3 path is not yet plugged
   into the i18n catalog.
   TODO(future): bridge i18n through a v3 port.

2. `/help` body still lives in legacy `helpfile.cpp` (file I/O on
   the topics corpus). Strangler step 1: v3 owns dispatch, legacy
   owns body.
   TODO(future): port the help corpus loader into v3 and reimplement
   `handle_help_command` purely.

3. The first-token parser in the router treats whitespace, CR, LF as
   delimiters. Legacy used `strstart` (prefix match). Equivalent for
   the migrated commands since none has another command as a prefix.

## Tests

- `test_application_admin_commands_router` -- 11 Catch2 cases:
  - `/version` and `/ver` resolve to canonical `/version`
  - `/uptime` resolves to canonical `/uptime`
  - `/help` and `/?` resolve to canonical `/help`
  - trailing argument tokens are ignored
  - leading whitespace is skipped
  - command token is case insensitive (`/VERSION`)
  - unrecognised commands -> NotFound
  - non-slash text -> NotFound
  - empty input -> NotFound
  - denied-by-predicate -> Denied with canonical name
  - predicate receives the canonical name, never the alias

## Verify

```
docker build -f Dockerfile.v3 --target v3-test -t pvpgn-v3-test:r216 .
```

Success marker: `naming to docker.io/library/pvpgn-v3-test:r216 done`.

## Follow-up rounds (not in this round)

- R216b: migrate the next batch of read-only commands (`/who`,
  `/whoami`, `/users`, `/finger`).
- R216c: bridge i18n so the v3 path can localize.
- R216d: extract the help corpus loader and reimplement
  `handle_help_command` in pure v3.
- ... eventually retire `src/bnetd/command.cpp` entirely.
