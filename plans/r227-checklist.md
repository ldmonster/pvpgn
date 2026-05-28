# R227 -- Account / admin-ops batch as strangler shells

Status: **GREEN** (`pvpgn-v3-test:r227`)

## Scope

11 canonical commands, no aliases:
/addacct /chpass /kill /killsession /find /save /set /rehash /config
/shutdown /serverban.

Bodies remain in legacy `command.cpp`.

## Files modified

- [src/bnetd/command.cpp](src/bnetd/command.cpp): 11 fwd decls + 11
  defs `static int` -> `extern int`. `_handle_config_command` def
  uses `char const * /*text*/` signature.
- [src/bnetd/command_legacy.h](src/bnetd/command_legacy.h): 11 extern
  decls under `// R227:`.
- [src/v3/application/admin_commands/src/router.cpp](src/v3/application/admin_commands/src/router.cpp):
  `kAliases` 83 -> 94.
- [src/v3/integration/legacy_bnetd/src/command_dispatch_bridge.cpp](src/v3/integration/legacy_bnetd/src/command_dispatch_bridge.cpp):
  11 `using` + 11 branches.
- [tests/unit/application/admin_commands/router_test.cpp](tests/unit/application/admin_commands/router_test.cpp):
  TEST_CASE "R227: account / admin-ops batch".

## Coverage so far

72 canonical commands, 94 alias rows.

## Verify

```
docker build -f Dockerfile.v3 --target v3-test -t pvpgn-v3-test:r227 .
```
