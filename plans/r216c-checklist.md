# R216c -- Wire the v3 command bridge into the legacy i18n catalog

Status: **GREEN** (`pvpgn-v3-test:r216c`)

## Scope

R216 left the v3 path for `/uptime` and the denial messages
("This command has been deactivated" / "This command is reserved
for admins.") in English-only. R216c restores full i18n parity by
routing those strings through the legacy `localize()` macro.

The compromise: i18n stays in legacy for now. A full extraction
(a v3 `IStringTable` adapter populated from
`conf/i18n/*/common.xml`) is a separate, larger round.

## Why this is safe

`localize(c, "Uptime: {}", x)` in `src/bnetd/i18n.h` expands to
`_localize(c, __FUNCTION__, "Uptime: {}", x)`. Looking inside
`_localize`, the catalog lookup is keyed off the format string
(`_find_string(format_str, lang)`) -- the `func` parameter is only
used for diagnostic logging. So invoking the macro from the bridge
yields the same catalog hit as invoking it from
`_handle_uptime_command` in `command.cpp`, and the translated
template for the user's selected `t_gamelang` is used.

## Files modified

- [src/v3/integration/legacy_bnetd/src/command_dispatch_bridge.cpp](src/v3/integration/legacy_bnetd/src/command_dispatch_bridge.cpp):
  - Added `#include "i18n.h"` between the other legacy headers.
  - `/uptime`: the literal `"Uptime: "` concat is replaced with
    `localize(c, "Uptime: {}", seconds_to_timestr(...))`.
  - `send_denied`: both messages now go through `localize()`.

## Catalog entries already exist

Verified the relevant entries are present in every locale file:

- `conf/i18n/common.xml`        -- "Uptime: {}"
- `conf/i18n/deDE/common.xml`   -- "Uptime: {}"
- `conf/i18n/esES/common.xml`   -- "Uptime: {}"
- Likewise "This command has been deactivated" and
  "This command is reserved for admins."

A non-English user on the v3 path now sees the same translated
text they did on the legacy path.

## Tests

No new tests; behaviour change is observable only against a populated
i18n catalog (Docker bring-up territory, not unit-test territory).
Router test suite continues to pass.

## Caveat

This round does NOT introduce a v3-native i18n adapter. The v3
`application::i18n::IStringTable` port exists (see
`application/i18n/include/.../string_table.hpp`) but is currently
unused by the command bridge. Routing the bridge through it would
require:
  1. Populating a `MapStringTable` from the legacy XML catalogs at
     bootstrap (XML parsing in v3 infra).
  2. An adapter to map `t_connection*` -> v3 locale tag.
  3. A small i18n facade that the bridge can `#include` without
     dragging `<fmt/format.h>` and legacy `i18n.h` through the
     bridge TU.

Deferred to a separate future round; the present round closes the
user-visible regression without doing that lift.

## Verify

```
docker build -f Dockerfile.v3 --target v3-test -t pvpgn-v3-test:r216c .
```

Success marker: `naming to docker.io/library/pvpgn-v3-test:r216c done`.
