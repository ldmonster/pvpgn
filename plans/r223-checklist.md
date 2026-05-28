# R223 -- Social/messaging commands as strangler shells

Status: **GREEN** (`pvpgn-v3-test:r223`)

## Scope

Continue the strangler-fig pattern with eight social/messaging
canonical commands (14 aliases total). v3 owns dispatch + permission
check; bodies remain in legacy `src/bnetd/command.cpp` (un-static'd,
declared in `command_legacy.h`).

| Canonical    | Aliases                       | Body                          |
|--------------|-------------------------------|-------------------------------|
| /clan        | /c                            | `_handle_clan_command`        |
| /friends     | /f                            | `_handle_friends_command`     |
| /me          | /emote                        | `_handle_me_command`          |
| /whisper     | /w, /m, /msg                  | `_handle_whisper_command`     |
| /watch       | -                             | `_handle_watch_command`       |
| /unwatch     | -                             | `_handle_unwatch_command`     |
| /tos         | -                             | `_handle_tos_command`         |
| /clearstats  | -                             | `_handle_clearstats_command`  |

## Files modified

- [src/bnetd/command.cpp](src/bnetd/command.cpp):
  - 8 forward declarations changed from `static int` to `extern int`.
  - 8 definitions changed from `static int` to `extern int`.
- [src/bnetd/command_legacy.h](src/bnetd/command_legacy.h):
  - Added 8 `extern int` declarations under an `// R223:` heading.
- [src/v3/application/admin_commands/src/router.cpp](src/v3/application/admin_commands/src/router.cpp):
  - `kAliases` array size 25 -> 39.
  - 14 new alias rows.
- [src/v3/integration/legacy_bnetd/src/command_dispatch_bridge.cpp](src/v3/integration/legacy_bnetd/src/command_dispatch_bridge.cpp):
  - 8 new `using` decls + 8 new single-line `run_handler` branches.
- [tests/unit/application/admin_commands/router_test.cpp](tests/unit/application/admin_commands/router_test.cpp):
  - New test case "R223: router resolves the social/messaging batch"
    covering all 14 alias -> canonical mappings.

## Coverage so far

33 canonical commands route through the v3 strangler (39 aliases):
/version, /uptime, /help, /who, /whoami, /users, /finger, /time,
/news, /games, /channels, /motd, /copyright, /lusers, /connections,
/admins, /quit, /beep, /nobeep, /away, /dnd, /squelch, /unsquelch,
/clan, /friends, /me, /whisper, /watch, /unwatch, /tos, /clearstats.

/help body is pure v3. The other 32 still delegate to legacy bodies.

## Verify

```
docker build -f Dockerfile.v3 --target v3-test -t pvpgn-v3-test:r223 .
```

Success marker: `naming to docker.io/library/pvpgn-v3-test:r223 done`.
