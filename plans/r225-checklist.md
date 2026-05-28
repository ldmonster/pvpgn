# R225 -- Channel rights / op commands as strangler shells

Status: **GREEN** (`pvpgn-v3-test:r225`)

## Scope

9 op/rights commands routed through v3, no aliases:
/admin /operator /aop /op /tmpop /deop /voice /devoice /vop.

Bodies remain in legacy `command.cpp`.

## Files modified

- [src/bnetd/command.cpp](src/bnetd/command.cpp): 9 fwd decls + 9
  defs `static int` -> `extern int`.
- [src/bnetd/command_legacy.h](src/bnetd/command_legacy.h): 9 extern
  decls under `// R225:`.
- [src/v3/application/admin_commands/src/router.cpp](src/v3/application/admin_commands/src/router.cpp):
  `kAliases` 61 -> 70.
- [src/v3/integration/legacy_bnetd/src/command_dispatch_bridge.cpp](src/v3/integration/legacy_bnetd/src/command_dispatch_bridge.cpp):
  9 `using` + 9 branches.
- [tests/unit/application/admin_commands/router_test.cpp](tests/unit/application/admin_commands/router_test.cpp):
  TEST_CASE "R225: channel rights / op batch".

## Coverage so far

52 canonical commands, 70 alias rows.

## Verify

```
docker build -f Dockerfile.v3 --target v3-test -t pvpgn-v3-test:r225 .
```
