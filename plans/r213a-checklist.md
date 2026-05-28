# R213a — Extend header self-containment to domain/shared, protocol/common, application/ports

Status: **GREEN ✓** (`pvpgn-v3-test:r213a`)

## Scope
Extend the R213 selfcheck pattern (one TU per header, must compile in isolation)
from `core/` to three more layers:

- `tests/unit/domain/shared/` — 10 headers under `domain/shared/`
- `tests/unit/protocol/common/` — 4 headers under `protocol/common/`
- `tests/unit/application/ports/` — 19 of 20 headers under `application/ports/`

## Changes
- `tests/unit/domain/shared/CMakeLists.txt`: added
  `test_domain_shared_headers_selfcontained` (10 headers; DEPS `domain_shared`).
- `tests/unit/protocol/common/CMakeLists.txt`: added
  `test_protocol_common_headers_selfcontained` (4 headers; DEPS
  `protocol_common`).
- `tests/unit/application/ports/CMakeLists.txt`: added
  `test_application_ports_headers_selfcontained` (19 headers; DEPS
  `application_ports` + domain_shared/identity/chat/social/gameplay/moderation/realm).
- `Dockerfile.v3`: added 3 new selfcheck targets to the v3-build `--target`
  list and 4 new run lines (one per new selfcheck) to v3-test.

## Findings
1. **Layering violation (deferred)** — `application/ports/config_subscriber.hpp`
   includes `infra/config/server_config.hpp`. This breaks the v3 layering rule
   (application MUST NOT depend on infra). The header is excluded from the
   selfcheck list with a `# R213a TODO:` comment pointing to the fix:
   introduce a domain/application-side `ConfigSnapshot` value type and have
   the infra config layer adapt `ServerConfig -> ConfigSnapshot`.
2. **Incomplete library DEPS (already worked around)** — `application_ports`
   itself only PUBLIC-links `domain_identity/chat/gameplay`. Many port
   headers transitively include `domain/shared/*`, `domain/social/*`,
   `domain/moderation/*`, `domain/realm/*`. Today this compiles only because
   callers happen to link those libs too. The selfcheck makes the missing
   transitive deps visible. Worth a future round to make
   `application_ports` declare all PUBLIC deps it actually needs.

## Verify
```
docker build -f Dockerfile.v3 --target v3-test -t pvpgn-v3-test:r213a .
```
Success marker: `naming to docker.io/library/pvpgn-v3-test:r213a done`.
