# R226 -- Moderation / account-state batch as strangler shells

Status: **GREEN** (`pvpgn-v3-test:r226`)

## Scope

9 canonical moderation/account-state commands (13 alias rows):

| Canonical    | Aliases    | Body                       |
|--------------|------------|----------------------------|
| /kick        | -          | `_handle_kick_command`     |
| /ban         | -          | `_handle_ban_command`      |
| /unban       | -          | `_handle_unban_command`    |
| /lockacct    | /lock      | `_handle_lockacct_command` |
| /unlockacct  | /unlock    | `_handle_unlockacct_command` |
| /muteacct    | /mute      | `_handle_muteacct_command` |
| /unmuteacct  | /unmute    | `_handle_unmuteacct_command` |
| /flag        | -          | `_handle_flag_command`     |
| /tag         | -          | `_handle_tag_command`      |

## Files modified

- [src/bnetd/command.cpp](src/bnetd/command.cpp): 9 fwd decls + 9
  defs `static int` -> `extern int`.
- [src/bnetd/command_legacy.h](src/bnetd/command_legacy.h): 9 extern
  decls under `// R226:`.
- [src/v3/application/admin_commands/src/router.cpp](src/v3/application/admin_commands/src/router.cpp):
  `kAliases` 70 -> 83.
- [src/v3/integration/legacy_bnetd/src/command_dispatch_bridge.cpp](src/v3/integration/legacy_bnetd/src/command_dispatch_bridge.cpp):
  9 `using` + 9 branches.
- [tests/unit/application/admin_commands/router_test.cpp](tests/unit/application/admin_commands/router_test.cpp):
  TEST_CASE "R226: moderation / account-state batch"; the legacy
  "/kick should be NotFound" test was rewritten to use a literal
  non-existent name (`/notarealcommand`) since /kick now routes.

## Coverage so far

61 canonical commands, 83 alias rows.

## Verify

```
docker build -f Dockerfile.v3 --target v3-test -t pvpgn-v3-test:r226 .
```
