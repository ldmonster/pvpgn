# R216b -- Second wave of legacy command strangler-fig migration

Status: **GREEN** (`pvpgn-v3-test:r216b`)

## Scope

Migrate four more legacy chat commands as v3-dispatch + legacy-body
shells. Commands chosen are the next-easiest read-only info batch.

| Command | Aliases   | Body still in       | Strangler step |
|---------|-----------|---------------------|----------------|
| /who    | (none)    | legacy `_handle_who_command`    | thin shell |
| /whoami | (none)    | legacy `_handle_whoami_command` | thin shell |
| /users  | /status   | legacy `_handle_status_command` | thin shell |
| /finger | (none)    | legacy `_handle_finger_command` | thin shell |

Note: `/users` is dispatched to `_handle_status_command` to match
legacy table behaviour exactly. We expose `/users` as the canonical
v3 name because that is what was requested; `/status` is not yet in
the v3 router (deferred to a later round; legacy still owns it
when called directly via `/status`).

## Files added

- [src/bnetd/command_legacy.h](src/bnetd/command_legacy.h) -- forward
  declarations for the four un-static'd legacy handlers so the v3
  bridge can call them by symbol.

## Files modified

- [src/bnetd/command.cpp](src/bnetd/command.cpp) -- the four legacy
  static functions (`_handle_status_command`, `_handle_who_command`,
  `_handle_whoami_command`, `_handle_finger_command`) and their
  forward declarations at the top of the file are now `extern` (no
  `static`). Added a comment block explaining why; bodies are
  unchanged.
- [src/v3/application/admin_commands/include/application/admin_commands/router.hpp](src/v3/application/admin_commands/include/application/admin_commands/router.hpp)
  -- documented new aliases in the header comment.
- [src/v3/application/admin_commands/src/router.cpp](src/v3/application/admin_commands/src/router.cpp)
  -- alias array now has 9 entries (5 original + 4 new).
- [src/v3/integration/legacy_bnetd/src/command_dispatch_bridge.cpp](src/v3/integration/legacy_bnetd/src/command_dispatch_bridge.cpp)
  -- includes `command_legacy.h`, adds four `run_handler` branches
  delegating to the corresponding legacy functions.
- [tests/unit/application/admin_commands/router_test.cpp](tests/unit/application/admin_commands/router_test.cpp)
  -- added test case "R216b: router resolves the read-only info
  batch"; switched the "unrecognised commands" probe from `/whoami`
  to `/kick` (since `/whoami` is now in the migrated set).

## Tests

`test_application_admin_commands_router` now has 12 test cases
(33 assertions), all passing. Five Catch2 cases cover the four new
commands plus their interaction with argument strings (e.g.
`/finger alice`, `/who #op`).

## Verify

```
docker build -f Dockerfile.v3 --target v3-test -t pvpgn-v3-test:r216b .
```

Success marker: `naming to docker.io/library/pvpgn-v3-test:r216b done`.
