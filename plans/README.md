# PvPGN Refactoring Plan — Index

This `plans/` folder is the single source of truth for the next wave of
refactoring work on the PvPGN repository.

The goal is to push the codebase toward a **solid, modern, DDD-shaped,
KISS+DRY+YAGNI, expandable and testable** server platform — building on the
v3 hexagonal-architecture work that already landed in `3.0.0`.

Read the files in order. Each file is independently actionable: a section
"What" / "Why" / "Concrete steps" / "Acceptance criteria" / "Risks" /
"Out of scope".

| # | File | Theme |
|---|------|-------|
| 00 | [overview.md](00-overview.md) | High-level vision and scope |
| 01 | [principles-and-acceptance.md](01-principles-and-acceptance.md) | DDD / KISS / DRY / YAGNI rules + Definition of Done |
| 02 | [repo-hygiene.md](02-repo-hygiene.md) | Delete useless files; reorganize top level |
| 03 | [config-toml-consolidation.md](03-config-toml-consolidation.md) | Collapse legacy `.conf` configs into TOML |
| 04 | [lua-to-scripts-migration.md](04-lua-to-scripts-migration.md) | Move `lua/` -> `scripts/lua/`, modernize Lua API consumers |
| 05 | [large-file-decomposition.md](05-large-file-decomposition.md) | Break up monster `.cpp`/`.h` files |
| 06 | [legacy-bnetd-strangler-completion.md](06-legacy-bnetd-strangler-completion.md) | Finish the strangler-fig over `src/integration/legacy_bnetd/` |
| 07 | [bounded-contexts-and-layering.md](07-bounded-contexts-and-layering.md) | DDD bounded contexts + layering enforcement |
| 08 | [testing-strategy.md](08-testing-strategy.md) | Unit / integration / e2e / fuzz pyramid |
| 09 | [build-system-modernization.md](09-build-system-modernization.md) | CMake, presets, vcpkg, CI |
| 10 | [observability-and-ops.md](10-observability-and-ops.md) | Logging, metrics, tracing, ops UX |
| 11 | [plugin-and-extensibility.md](11-plugin-and-extensibility.md) | Plugin ABI, scripting, expansion points |
| 12 | [docs-overhaul.md](12-docs-overhaul.md) | `docs/` cleanup, ADRs, generated reference |
| 13 | [execution-roadmap.md](13-execution-roadmap.md) | Ordering, milestones, rollback strategy |

Progress should be tracked in `refactoring-progress.md` at the repo root
(per `prompt.txt`). Each plan file ends with a numbered checklist that can
be ticked off there.
