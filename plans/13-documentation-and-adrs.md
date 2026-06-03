# 13 — Documentation & ADRs

Documentation is maintainable only when it is **generated from source** wherever
possible and **gated locally** so it cannot rot. PvPGN already has `mkdocs.yml`,
a `docs/` tree with ADRs, and generators in `scripts/dev/`.

## 1. What is generated, not hand-written (DRY)

| Doc | Source of truth | Generator | Sync gate |
|-----|-----------------|-----------|-----------|
| Config reference | config schema in `core/config` | `gen-config-docs.sh` | `check-config-reference-sync.sh` |
| Plugin API | `include/pvpgn/plugin` ABI | `gen-plugin-docs.sh` | `check-plugin-abi.sh` |
| Lua API | Lua binding tables | `gen-lua-docs.sh` | `.luacheckrc` + gen diff |
| Domain language | `domain/<ctx>` headers | curated `docs/domain/<ctx>.md` | review |

A doc that duplicates a fact already in code must be generated from it, never
maintained by hand.

## 2. Architecture Decision Records

- ADRs live in `docs/adr/NNNN-title.md`, append-only, numbered.
- Every significant decision in this plan gets an ADR: the dependency rule, the
  `IDbDriver` consolidation, the crypto/`hash_version` policy, the plugin C ABI,
  the async-runtime choice, the "local-only gates, no CI" stance.
- ADRs record *context → decision → consequences*, including what we explicitly
  rejected (YAGNI rationale) so future readers don't re-litigate.

## 3. Living architecture docs

- `docs/architecture/` holds the layer diagram, the dependency rule, and the
  ports/adapters catalogue — kept in step with `v3_layering_check.sh` (if the
  doc and the script disagree, that's a bug in one of them).
- A "how to add X" page per extension point (backend, protocol message, auth
  scheme, plugin) — short, concrete, pointing at the real seam.

## 4. Contributor docs

- `CONTRIBUTING.md`: the three rings of gates ([11](11-local-quality-gates.md)),
  the precedence rule (KISS/YAGNI > DRY), the "skip honestly, never fake green"
  rule, and the one-step-at-a-time workflow.
- `CHANGELOG.md`: Keep-a-Changelog, enforced by `check-changelog.sh` in
  pre-commit.

## 5. The strict build gate (local)

- `mkdocs build --strict` must be green: no broken links, no missing nav
  entries, no orphan pages (`check-docs-reachable.sh`).
- Run it in `check-all.sh` so docs breakage is caught before push, not in a
  pipeline.

## 6. Consolidate the trackers

- Replace the scattered `refactoring-progress*.md` + `planstwo/` reports with a
  **single** `docs/refactoring/progress.md` keyed to this plan's sections.
  Archive the old reports under `docs/refactoring/archive/` for history.

## 7. Tasks for this plan

1. Move/curate per-context ubiquitous-language docs under `docs/domain/`.
2. Backfill ADRs for the decisions in §2 that aren't yet recorded.
3. Wire `mkdocs build --strict` + doc generators + sync checks into
   `check-all.sh`.
4. Collapse trackers into one progress file; archive the rest.

## Definition of Done

- [ ] `mkdocs build --strict` is green and runs in `check-all.sh`.
- [ ] Config/plugin/Lua reference docs are generated and their sync checks pass.
- [ ] Every §2 decision has an ADR.
- [ ] A single progress tracker exists; legacy trackers are archived.
