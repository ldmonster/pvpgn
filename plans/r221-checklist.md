# R221 -- Server-info batch of legacy commands as strangler shells

Status: **GREEN** (`pvpgn-v3-test:r221`)

## Scope

Same shell pattern as R216b / R220. v3 owns dispatch + permission
check; bodies stay in legacy `src/bnetd/command.cpp` exposed via
`command_legacy.h`.

| Command       | Body in (legacy)                |
|---------------|---------------------------------|
| /copyright    | `_handle_copyright_command`     |
| /lusers       | `_handle_lusers_command`        |
| /connections  | `_handle_connections_command`   |
| /admins       | `_handle_admins_command`        |

All four are read-only info commands -- safe to delegate from a
single strangler hop.

## Files modified

- [src/bnetd/command.cpp](src/bnetd/command.cpp):
  - 4 forward declarations changed from `static int` to `extern int`.
  - 4 definitions changed from `static int` to `extern int`.
- [src/bnetd/command_legacy.h](src/bnetd/command_legacy.h):
  - Added the 4 `extern int` declarations under an `// R221:` heading.
- [src/v3/application/admin_commands/src/router.cpp](src/v3/application/admin_commands/src/router.cpp):
  - `kAliases` array size 14 -> 18.
- [src/v3/integration/legacy_bnetd/src/command_dispatch_bridge.cpp](src/v3/integration/legacy_bnetd/src/command_dispatch_bridge.cpp):
  - 4 new `using` declarations + 4 new single-line `run_handler`
    branches.
- [tests/unit/application/admin_commands/router_test.cpp](tests/unit/application/admin_commands/router_test.cpp):
  - New test case "R221: router resolves the server-info batch".

## Verify

```
docker build -f Dockerfile.v3 --target v3-test -t pvpgn-v3-test:r221 .
```

Success marker: `naming to docker.io/library/pvpgn-v3-test:r221 done`.
