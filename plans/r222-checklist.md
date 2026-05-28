# R222 -- Per-session state commands as strangler shells

Status: **GREEN** (`pvpgn-v3-test:r222`)

## Scope

Continue the strangler-fig pattern with seven per-session state
commands. v3 owns dispatch + permission check; bodies remain in
legacy `src/bnetd/command.cpp` (un-static'd, declared in
`command_legacy.h`).

| Command     | Body in (legacy)            |
|-------------|-----------------------------|
| /quit       | `_handle_quit_command`      |
| /beep       | `_handle_beep_command`      |
| /nobeep     | `_handle_nobeep_command`    |
| /away       | `_handle_away_command`      |
| /dnd        | `_handle_dnd_command`       |
| /squelch    | `_handle_squelch_command`   |
| /unsquelch  | `_handle_unsquelch_command` |

These are state-changing but per-session and well-bounded. None
require pure-v3 ports beyond what the bridge already provides.

## Files modified

- [src/bnetd/command.cpp](src/bnetd/command.cpp):
  - 7 forward declarations changed from `static int` to `extern int`.
  - 7 definitions changed from `static int` to `extern int`.
- [src/bnetd/command_legacy.h](src/bnetd/command_legacy.h):
  - Added the 7 `extern int` declarations under an
    `// R222: per-session state commands.` heading.
- [src/v3/application/admin_commands/src/router.cpp](src/v3/application/admin_commands/src/router.cpp):
  - `kAliases` array size 18 -> 25.
- [src/v3/integration/legacy_bnetd/src/command_dispatch_bridge.cpp](src/v3/integration/legacy_bnetd/src/command_dispatch_bridge.cpp):
  - 7 new `using` declarations + 7 new single-line `run_handler`
    branches.
- [tests/unit/application/admin_commands/router_test.cpp](tests/unit/application/admin_commands/router_test.cpp):
  - New test case "R222: router resolves the per-session state batch".

## Coverage so far

25 chat commands route through the v3 strangler:
/version, /ver, /uptime, /help, /?, /who, /whoami, /users, /finger,
/time, /news, /games, /channels, /motd, /copyright, /lusers,
/connections, /admins, /quit, /beep, /nobeep, /away, /dnd, /squelch,
/unsquelch.

/help body is pure v3 (`FileHelpResponder`); the other 24 still call
legacy bodies through the un-static'd handler hops.

## Verify

```
docker build -f Dockerfile.v3 --target v3-test -t pvpgn-v3-test:r222 .
```

Success marker: `naming to docker.io/library/pvpgn-v3-test:r222 done`.
