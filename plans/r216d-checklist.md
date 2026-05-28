# R216d -- IHelpResponder port + legacy adapter for /help

Status: **GREEN** (`pvpgn-v3-test:r216d`)

## Scope

Move the `/help` body call out of the bridge body and behind a port,
without changing runtime behaviour. The bridge no longer references
`handle_help_command` directly; it talks to an
`application::admin_commands::IHelpResponder`, which the
`integration_legacy_bnetd_linked` layer wires up to a concrete
`LegacyHelpResponder` adapter that delegates back into legacy
`handle_help_command`.

This is a pure structural step. It draws the port boundary that a
future round needs in order to introduce a pure-v3 `FileHelpResponder`
(parsing the help corpus directly without any legacy dependency).

## Architecture

```
        bridge -- run_handler() for "/help"
                              |
                              v
        IHelpResponder::respond(conn, text)        <- v3 port
                              |
                              v
        LegacyHelpResponder::respond  (R216d)      <- in _linked
                              |
                              v
        legacy handle_help_command(c, text)        <- helpfile.cpp
                              |
                              v
        legacy file I/O + message_send_text
```

A later round can replace `LegacyHelpResponder` with `FileHelpResponder`
(pure v3) without touching the bridge or the router.

## Files added

- [src/v3/application/admin_commands/include/application/admin_commands/help_responder.hpp](src/v3/application/admin_commands/include/application/admin_commands/help_responder.hpp)
  -- the `IHelpResponder` port. Connection handle is `void*` so the
  port header stays free of legacy types.
- [src/v3/integration/legacy_bnetd/include/integration/legacy_bnetd/legacy_help_responder.hpp](src/v3/integration/legacy_bnetd/include/integration/legacy_bnetd/legacy_help_responder.hpp)
  -- `LegacyHelpResponder` class declaration.
- [src/v3/integration/legacy_bnetd/src/legacy_help_responder.cpp](src/v3/integration/legacy_bnetd/src/legacy_help_responder.cpp)
  -- thin adapter: casts the `void*` back to
  `pvpgn::bnetd::t_connection*` and calls
  `handle_help_command(c, text.c_str())`.

## Files modified

- [src/v3/integration/legacy_bnetd/CMakeLists.txt](src/v3/integration/legacy_bnetd/CMakeLists.txt)
  -- added `src/legacy_help_responder.cpp` to the `_linked` SOURCES.
- [src/v3/integration/legacy_bnetd/src/command_dispatch_bridge.cpp](src/v3/integration/legacy_bnetd/src/command_dispatch_bridge.cpp):
  - Removed `#include "helpfile.h"` and the
    `using ::pvpgn::bnetd::handle_help_command;` line.
  - Added `#include "integration/legacy_bnetd/legacy_help_responder.hpp"`.
  - The `/help` branch in `run_handler` now constructs (lazily, via
    `static const`) a `LegacyHelpResponder` and calls `respond` on it.

## What this round does NOT do

- Does not write a `FileHelpResponder` (pure-v3 corpus parser).
- Does not move the help corpus i18n strings into a v3 catalog.
- Does not register the responder via DI; the bridge owns a single
  static instance because there is exactly one production
  implementation today.

These are the explicit next steps and are deferred to a future
round (call it R216e). The bridge surface they will use already
exists.

## Tests

The router test suite is unchanged (router doesn't know about the
responder). The structural change is verified by the Docker build
linking successfully -- no `handle_help_command` symbol references
from `command_dispatch_bridge.cpp` survive.

## Verify

```
docker build -f Dockerfile.v3 --target v3-test -t pvpgn-v3-test:r216d .
```

Success marker: `naming to docker.io/library/pvpgn-v3-test:r216d done`.
