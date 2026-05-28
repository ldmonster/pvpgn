# R217 — CI layering enforcement

Status: **GREEN ✓** (`pvpgn-v3-test:r217`)

## What was added
- `scripts/v3_layering_check.sh` — POSIX shell script that scans every
  `.hpp`/`.cpp`/`.h` under `src/v3/<layer>/` for forbidden `#include`
  statements. Layering rules enforced:

  | Layer | Forbidden lower layers |
  |---|---|
  | core | domain, application, protocol, infra, integration, services, app |
  | domain | application, protocol, infra, integration, services, app |
  | application | infra, integration, services, app |
  | protocol | infra, integration, services, app |
  | infra | integration, services, app |
  | integration | services, app |
  | services | app |

- `Dockerfile.v3`: new stage **`v3-layer-check`** between `v3-base` and
  `v3-build`. The check runs on every R<NNN> build, and any new violation
  fails the build before CMake configure.

## Findings (caught by the new check on first run)
1. **`application/ports/include/application/ports/config_subscriber.hpp:7`** ->
   `#include "infra/config/server_config.hpp"`. Already known from R213a;
   fix is to introduce a domain/application-side `ConfigSnapshot` value
   type and an infra adapter.
2. **`application/anongame_infoply/src/inforeply_builder.cpp:7`** ->
   `#include "infra/compression/zlib_anongame.hpp"`. New finding -- the
   application layer compresses data directly. Fix is to define an
   `ICompressor` port in `application/ports/` and inject the zlib adapter.

Both violations are present in the script's allow-list as
`"path|header"` substring entries. Each entry corresponds to a future
refactor round; the allow-list is the single source of truth for accepted
debt.

## How to add a new allowed violation (DO NOT use lightly)
Edit the `ALLOW_FILE` heredoc in `scripts/v3_layering_check.sh` and add
a line: `path/from/repo/root.cpp|forbidden/header.hpp`. New entries
require owner review.

## Verify
```
docker build -f Dockerfile.v3 --target v3-layer-check -t pvpgn-v3-layer-check:r217 .
docker build -f Dockerfile.v3 --target v3-test        -t pvpgn-v3-test:r217        .
```
Success markers:
- `v3_layering_check: OK` printed during `v3-layer-check` stage
- `naming to docker.io/library/pvpgn-v3-test:r217 done`
