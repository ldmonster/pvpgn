# 12 — Documentation overhaul

## What

`docs/` is currently a mix of build instructions, historical surveys, and reference. Reorganize by audience so each page has one reader.

## Target layout

```
docs/
  index.md                      # entry: 30-second pitch + nav

  user/
    install.md
    config-reference.md         # auto-generated from server_config.hpp
    bnmotd.md
    versioncheck.md
    adbanners.md
    observability.md
    ports.md

  operator/
    building.md                 # consolidated compile guide
    upgrading.md
    toml-migration.md
    backups.md
    troubleshooting.md

  developer/
    architecture.md             # DDD overview + diagrams
    bounded-contexts.md
    layering.md
    testing.md
    extending-pvpgn.md          # the canonical "add a feature" walkthrough
    plugin-versioning-guide.md
    sandbox-integration-guide.md
    lua-api-v2.md
    single-binary-mode.md

  reference/
    cli.md                      # bnetd, pvpgn-migrate, pvpgn-config
    metrics.md                  # generated metric catalogue
    events.md                   # domain events catalogue
    errors.md                   # StatusCode catalogue

  adr/                          # architecture decisions
    0001-hexagonal-architecture.md
    0002-toml-only-config.md
    0003-strangler-fig-for-legacy-bnetd.md
    0004-lua-api-v2.md
    0005-c++20-baseline.md
    NNNN-template.md

  history/                      # historical surveys
    alloc-survey.md
    compat-survey.md
    migration-xalloc-to-stl.md
    fdwatch.md
    storage.md
```

## Concrete steps

1. `git mv` per the table above.
2. Convert `fdwatch.txt`, `storage.txt` to Markdown.
3. Rewrite `docs/readme.md` → `docs/index.md` with the new nav.
4. Add `mkdocs.yml` `nav:` matching the structure.
5. Generate `config-reference.md` from `server_config.hpp` via a small Python script in `scripts/dev/gen_config_reference.py`. Wire into CI as a "must be up to date" check.
6. Generate `metrics.md` and `errors.md` from source code annotations.
7. Add ADR template; backfill ADRs 0001–0005 from existing decisions.
8. Update every `[link](old-path.md)` across the repo.

## Acceptance criteria

- [ ] `mkdocs build --strict` is clean.
- [ ] Every page in `docs/` is reachable from `docs/index.md` in ≤ 3 clicks.
- [ ] CI fails when `config-reference.md` is out of sync with the schema.
- [ ] No top-level `docs/*.md` remains except `index.md`.

## Risks

- External links to old `docs/*.md` URLs break. Add a static redirect map in `mkdocs.yml` (`plugins.redirects`).

## Out of scope

- A new doc generator (e.g. Doxygen for C++ API). Keep mkdocs.
