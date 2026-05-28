# R228 -- Info / network / misc batch as strangler shells

Status: **GREEN** (`pvpgn-v3-test:r228`)

## Scope

11 canonical + 6 aliases (17 alias rows):

| Canonical        | Aliases             | Body                              |
|------------------|---------------------|-----------------------------------|
| /stats           | /astat              | `_handle_stats_command`           |
| /whois           | /whereis, /where    | `_handle_whois_command`           |
| /gameinfo        | -                   | `_handle_gameinfo_command`        |
| /ladderactivate  | -                   | `_handle_ladderactivate_command`  |
| /ladderinfo      | -                   | `_handle_ladderinfo_command`      |
| /timer           | -                   | `_handle_timer_command`           |
| /netinfo         | -                   | `_handle_netinfo_command`         |
| /quota           | -                   | `_handle_quota_command`           |
| /ipscan          | -                   | `_handle_ipscan_command`          |
| /commandgroups   | /cg                 | `_handle_commandgroups_command`   |
| /ping            | /p, /latency        | `_handle_ping_command`            |

## Files modified

- [src/bnetd/command.cpp](src/bnetd/command.cpp): 11 fwd decls + 11
  defs `static int` -> `extern int`.
- [src/bnetd/command_legacy.h](src/bnetd/command_legacy.h): 11 extern
  decls under `// R228:`.
- [src/v3/application/admin_commands/src/router.cpp](src/v3/application/admin_commands/src/router.cpp):
  `kAliases` 94 -> 111.
- [src/v3/integration/legacy_bnetd/src/command_dispatch_bridge.cpp](src/v3/integration/legacy_bnetd/src/command_dispatch_bridge.cpp):
  11 `using` + 11 branches.
- [tests/unit/application/admin_commands/router_test.cpp](tests/unit/application/admin_commands/router_test.cpp):
  TEST_CASE "R228: info / network / misc batch".

## Coverage so far

83 canonical commands, 111 alias rows. The remaining legacy entries
have non-`_handle_*` bodies (`handle_mail_command`, `handle_icon_command`,
`handle_ipban_command`, `handle_help_command`, `handle_language_command`,
`handle_log_command`) and need a separate adapter pass.

## Verify

```
docker build -f Dockerfile.v3 --target v3-test -t pvpgn-v3-test:r228 .
```
