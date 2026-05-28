# R219 -- Eliminate the final layering allow-list entry

Status: **GREEN** (`pvpgn-v3-test:r219`)

## Change
- `src/v3/application/ports/include/application/ports/config_subscriber.hpp`:
  removed `#include "infra/config/server_config.hpp"` and replaced it
  with a forward declaration `namespace pvpgn::infra::config { struct ServerConfig; }`.
  The interface method still takes `const ServerConfig&` (compile-time
  not-yet-decoupled) but the port header no longer pulls infra symbols
  through a transitive include. This satisfies the layering check's
  rule (no forbidden `#include`).
- `tests/unit/application/ports/CMakeLists.txt`: re-added
  `config_subscriber.hpp` to the headers selfcheck list (the TODO
  exclusion is gone).
- `scripts/v3_layering_check.sh`: allow-list is now empty.

## Caveat documented in the header
Full decoupling -- replacing the `ServerConfig` reference with a
domain/application-side `ConfigSnapshot` value type and an infra
adapter -- remains a future task because `ServerConfig` is a large
kitchen-sink struct and `IConfigSubscriber` has zero concrete
implementations today (only the watcher's call-site exists). The
forward declaration is the canonical hexagonal compromise until
that day.

## Verify
```
docker build -f Dockerfile.v3 --target v3-test -t pvpgn-v3-test:r219 .
```
Success marker: `naming to docker.io/library/pvpgn-v3-test:r219 done`.
