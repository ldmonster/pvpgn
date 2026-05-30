# 07 — DDD bounded contexts and layering

## What

Lock in the bounded contexts that already exist in `src/domain/` and route every feature through them. Add CI enforcement so accidental cross-context imports fail the build, not the review.

## Current contexts

```
src/domain/
  chat/         # channels, whispers, topics
  connection/   # session lifecycle, presence
  d2cs/         # Diablo II character server domain
  d2dbs/        # Diablo II database server domain
  gameplay/     # game listings, hosting, results
  identity/     # accounts, auth, profile (BNet hash, SRP)
  ladder/       # rankings, seasons
  matchmaking/  # anongame, AT/PG matching
  moderation/   # bans, mutes, ipbans
  realm/        # D2 realm registry
  shared/       # cross-cutting value objects (Username, Tag, ClientVersion)
  social/       # friends, clans, mail
```

## Per-context skeleton

Every context conforms to:

```
src/domain/<ctx>/
  include/domain/<ctx>/
    aggregates.hpp     # entities + invariants
    value_objects.hpp  # immutable values
    events.hpp         # domain events
    ports.hpp          # interfaces only; method = port name
    errors.hpp         # context-specific StatusCode bridging
  src/
    *.cpp              # invariants + factories only
  CMakeLists.txt       # depends on: core
```

Application layer mirrors it:

```
src/application/<feature>/
  include/application/<feature>/<feature>.hpp   # use-case command/query types
  src/<feature>.cpp                              # orchestration only
  CMakeLists.txt                                 # depends on: domain/<ctx>, core
```

Infrastructure layer:

```
src/infra/<tech>/
  include/infra/<tech>/<thing>.hpp
  src/<thing>.cpp
  CMakeLists.txt    # depends on: domain (for port impls), core
```

## Renames / merges

- `src/application/admin_commands/` → split into `application/moderation/` (kick, ban) and `application/chat/` (broadcast, topic) — admin is a *role*, not a context.
- `src/application/anongame_infoply/` → typo; rename `anongame_inforeply/`.
- `src/application/init/` → fold into `app/bnetd/bootstrap/` (it's wire-up, not a use case).
- `src/application/email_management/` → fold into `application/identity/email/`.
- `src/application/i18n/` → move under `src/services/i18n/` (cross-cutting).
- `src/application/ports/` → ports belong in `domain/<ctx>/ports.hpp`; delete this folder.
- `src/application/bnet_packet_pump/` → move under `src/integration/bnet_packet_pump/` (it's a wire-up, not a use case).

## Layering check

Promote `scripts/v3_layering_check.sh` from advisory to required:

1. Parse `#include "x/y/z..."` lines.
2. For each, map `x` to a layer and assert it's allowed per plan 01.
3. Run in CI as `lint-layering` job; failure blocks merge.
4. Add an allowlist file `cmake/layering_exceptions.txt` for the strangler bridges only; entries auto-expire (commit message must include `Layering-Exception-Expires: YYYY-MM-DD`).

## Acceptance criteria

- [ ] Every `src/domain/<ctx>/` matches the skeleton.
- [ ] `lint-layering` is required for merge.
- [ ] `cmake/layering_exceptions.txt` is empty after plan 06 completes.
- [ ] `application/ports/` directory does not exist.

## Risks

- The `r246` / `r247` lifecycle bridges currently violate the rule. Track in the exceptions file with a defined expiry tied to plan 06 milestones.

## Out of scope

- Splitting `shared/` further. Keep it intentionally small; if it grows past 10 types, revisit.
