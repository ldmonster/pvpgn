# 05 — `application/ports/` Consolidation

## What

Delete `src/application/ports/`. Move every port interface to the
bounded context that owns it: `src/domain/<ctx>/ports/`. Update all
consumers.

## Why

- Wave-one plan 07 explicitly required no global `application/ports/`
  directory. It came back. Today it contains at least
  `realm_repository.hpp`, `permission_checker.hpp`,
  `command_registry.hpp`, and a re-export of
  `core/metrics.hpp`.
- A global ports bucket invites cross-context reach-through and
  breaks the DDD invariant that the aggregate owns its seams.

## Prerequisites

- None. This is mechanical and can run in parallel with plans 02–04.

## Concrete steps

1. **Inventory.** List every header under
   `src/application/ports/`. For each, identify the owning bounded
   context (look at which domain aggregate's invariants the port
   serves).
2. **Move.** `git mv` each port header to
   `src/domain/<ctx>/ports/`. Example:
   - `realm_repository.hpp` → `src/domain/realm/ports/realm_repository.hpp`
   - `permission_checker.hpp` → `src/domain/moderation/ports/permission_checker.hpp`
   - `command_registry.hpp` → `src/domain/chat/ports/command_registry.hpp`
3. **Re-export shim.** For the metrics re-export, just delete it;
   consumers include `core/metrics.hpp` directly.
4. **Update includes.** Run a workspace replace; verify with the
   layering check.
5. **Delete** `src/application/ports/` and its `CMakeLists.txt`.
6. **Lint rule.** Add a check to `scripts/v3_layering_check.sh` that
   fails on any file under `src/application/ports/`.

## Acceptance criteria

- [ ] `src/application/ports/` does not exist.
- [ ] Every port header lives under `src/domain/<ctx>/ports/`.
- [ ] Layering check fails fast if `application/ports/` reappears.
- [ ] All unit tests pass without source changes beyond include paths.

## Risks

- Moving a port may surface a hidden circular include between two
  contexts. Resolve by extracting a shared value object into
  `domain/shared/` rather than re-introducing a global ports bucket.

## Out of scope

- Renaming or redesigning the port interfaces themselves.
