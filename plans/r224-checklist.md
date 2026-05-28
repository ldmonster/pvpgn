# R224 -- Channel/chat-ops batch + backfill aliases

Status: **GREEN** (`pvpgn-v3-test:r224`)

## Scope

10 new channel/chat-ops canonical commands routed through the v3
strangler (14 alias rows). Bodies remain in legacy `command.cpp`.

| Canonical    | Aliases     | Body                          |
|--------------|-------------|-------------------------------|
| /channel     | /join, /j   | `_handle_channel_command`     |
| /rejoin      | -           | `_handle_rejoin_command`      |
| /topic       | -           | `_handle_topic_command`       |
| /moderate    | -           | `_handle_moderate_command`    |
| /announce    | /ann        | `_handle_announce_command`    |
| /reply       | /r          | `_handle_reply_command`       |
| /realmann    | -           | `_handle_realmann_command`    |
| /watchall    | -           | `_handle_watchall_command`    |
| /unwatchall  | -           | `_handle_unwatchall_command`  |
| /alert       | -           | `_handle_alert_command`       |

## Backfill aliases for previously-routed canonicals

| Alias       | Canonical (round) |
|-------------|-------------------|
| /warranty   | /copyright (R221) |
| /license    | /copyright (R221) |
| /ignore     | /squelch    (R222)|
| /unignore   | /unsquelch  (R222)|
| /logout     | /quit       (R222)|
| /exit       | /quit       (R222)|
| /con        | /connections (R221)|
| /chs        | /channels   (R220)|

## Files modified

- [src/bnetd/command.cpp](src/bnetd/command.cpp): 10 fwd decls + 10
  definitions `static int` -> `extern int`.
- [src/bnetd/command_legacy.h](src/bnetd/command_legacy.h): added 10
  `extern int` declarations under `// R224:`.
- [src/v3/application/admin_commands/src/router.cpp](src/v3/application/admin_commands/src/router.cpp):
  `kAliases` size 39 -> 61.
- [src/v3/integration/legacy_bnetd/src/command_dispatch_bridge.cpp](src/v3/integration/legacy_bnetd/src/command_dispatch_bridge.cpp):
  10 new `using` decls + 10 new `run_handler` branches.
- [tests/unit/application/admin_commands/router_test.cpp](tests/unit/application/admin_commands/router_test.cpp):
  added 2 TEST_CASEs ("R224: channel/chat-ops batch", "R224: backfill
  aliases").

## Coverage so far

43 canonical commands routed (61 alias rows in router):
R216b: /version /uptime /help /who /whoami /users /finger
R220:   /time /news /games /channels /motd
R221:   /copyright /lusers /connections /admins
R222:   /quit /beep /nobeep /away /dnd /squelch /unsquelch
R223:   /clan /friends /me /whisper /watch /unwatch /tos /clearstats
R224:   /channel /rejoin /topic /moderate /announce /reply /realmann
        /watchall /unwatchall /alert

Only /help body is pure v3; the other 42 still delegate.

## Verify

```
docker build -f Dockerfile.v3 --target v3-test -t pvpgn-v3-test:r224 .
```
