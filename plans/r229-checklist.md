# R229 -- Cross-TU body commands as strangler shells

Status: **GREEN** (`pvpgn-v3-test:r229`)

## Scope

Final batch — 5 commands whose bodies live OUTSIDE `command.cpp`
(they were already extern-visible). 6 alias rows total.

| Canonical    | Aliases | Body                                |
|--------------|---------|-------------------------------------|
| /mail        | -       | `handle_mail_command` (mail.cpp)    |
| /icon        | -       | `handle_icon_command` (icons.cpp)   |
| /ipban       | -       | `handle_ipban_command` (ipban.cpp)  |
| /language    | /lang   | `handle_language_command` (i18n.cpp)|
| /log         | -       | `handle_log_command` (userlog.cpp)  |

Names use the `handle_*` prefix (no leading underscore) -- they were
always public to the legacy build because they're called from
`standard_command_table` across TU boundaries.

## Files modified

- [src/bnetd/command_legacy.h](src/bnetd/command_legacy.h): 5
  `extern int handle_*` decls under `// R229:`. `command.cpp` itself
  was not touched (no static handlers to un-static).
- [src/v3/application/admin_commands/src/router.cpp](src/v3/application/admin_commands/src/router.cpp):
  `kAliases` 111 -> 117.
- [src/v3/integration/legacy_bnetd/src/command_dispatch_bridge.cpp](src/v3/integration/legacy_bnetd/src/command_dispatch_bridge.cpp):
  5 `using` + 5 branches.
- [tests/unit/application/admin_commands/router_test.cpp](tests/unit/application/admin_commands/router_test.cpp):
  TEST_CASE "R229: cross-TU bodies batch".

## Coverage so far

88 canonical commands, 117 alias rows. **All entries in legacy
`standard_command_table` are now routed through the v3 strangler.**
Only `/help` is dispatched by a pure-v3 body; the other 87 still
delegate to legacy bodies through un-static'd handler hops or
public extern bodies.

## Verify

```
docker build -f Dockerfile.v3 --target v3-test -t pvpgn-v3-test:r229 .
```
