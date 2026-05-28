# R220 -- Next batch of legacy read-only commands as strangler shells

(R217..R219 were used by a previous session; this round picks up at R220.)

Status: **GREEN** (`pvpgn-v3-test:r220`)

## Scope

Bring five more legacy read-only chat commands under v3 dispatch
using the same shell pattern as R216b. v3 owns the routing /
permission decision; the bodies still live in legacy
`src/bnetd/command.cpp`, exposed via `command_legacy.h`.

| Command   | Body in (legacy)              |
|-----------|-------------------------------|
| /time     | `_handle_time_command`        |
| /news     | `_handle_news_command`        |
| /games    | `_handle_games_command`       |
| /channels | `_handle_channels_command`    |
| /motd     | `_handle_motd_command`        |

## Files modified

- [src/bnetd/command.cpp](src/bnetd/command.cpp):
  - 5 forward declarations changed from `static int` to `extern int`.
  - 5 definitions changed from `static int` to `extern int`.
  - The R216b explanatory comment now reads "R216b / R220".
- [src/bnetd/command_legacy.h](src/bnetd/command_legacy.h):
  - Added the 5 `extern int` declarations under an `// R220:` heading.
- [src/v3/application/admin_commands/src/router.cpp](src/v3/application/admin_commands/src/router.cpp):
  - `kAliases` array size 9 -> 14. Five new
    `{name, name}` entries appended.
- [src/v3/integration/legacy_bnetd/src/command_dispatch_bridge.cpp](src/v3/integration/legacy_bnetd/src/command_dispatch_bridge.cpp):
  - 5 new `using` declarations inside the anonymous namespace.
  - 5 new single-line `run_handler` branches that delegate to the
    respective legacy handlers.
- [tests/unit/application/admin_commands/router_test.cpp](tests/unit/application/admin_commands/router_test.cpp):
  - Added test case "R220: router resolves the next read-only batch"
    iterating over all five new canonical names.

## Tests

`test_application_admin_commands_router` -- 13 cases total (one new
TEST_CASE that loops over all five new canonical names). Other
admin_commands tests (`router`, `help_corpus_parser`,
`file_help_responder`) continue to pass.

## Verify

```
docker build -f Dockerfile.v3 --target v3-test -t pvpgn-v3-test:r220 .
```

Success marker: `naming to docker.io/library/pvpgn-v3-test:r220 done`.
