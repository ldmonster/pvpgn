# R350 — Plugin/Lua Docs Generators — Checklist

## Objective
Provide automated documentation generation for the native plugin C ABI and the
Lua API v2 surface, with a GitHub Actions workflow that keeps `docs/` in sync on
every push to `main`.

---

## Deliverables

### `scripts/dev/gen-plugin-docs.sh`
- [x] `#!/usr/bin/env bash` + `set -euo pipefail`
- [x] Reads `src/v3/infra/plugin/include/infra/plugin/api.h`
- [x] Extracts `/** … */` Doxygen-style block comments
- [x] Outputs `docs/plugin-api.md` with YAML front-matter
- [x] Auto mode (default), doxygen mode (`--doxygen`), awk mode (`--awk`)
- [x] Prints summary line on success

### `scripts/dev/gen-lua-docs.sh`
- [x] `#!/usr/bin/env bash` + `set -euo pipefail`
- [x] Reads `src/v3/infra/scripting/src/lua_api_v2.cpp`
- [x] Extracts `// ----` separator comments and `pvpgn.set_function(...)` calls
- [x] Outputs `docs/lua-api-reference.md` with YAML front-matter
- [x] Prints summary line on success

### `.github/workflows/v3-docs.yml`
- [x] Trigger: `push` to `main` + `workflow_dispatch`
- [x] Permissions: `contents: write`, `pages: write`, `id-token: write`
- [x] Step: checkout with `fetch-depth: 0`
- [x] Step: set up Python 3.12
- [x] Step: install `mkdocs`, `mkdocs-material`, `mkdocs-git-revision-date-localized-plugin`
- [x] Step: `bash scripts/dev/gen-config-docs.sh`
- [x] Step: `bash scripts/dev/gen-plugin-docs.sh`
- [x] Step: `bash scripts/dev/gen-lua-docs.sh`
- [x] Step: detect `git diff --quiet docs/` → set `changed=true` output
- [x] Step: commit updated `docs/` with `[skip ci]` message (only if changed)
- [x] Step: `mkdocs gh-deploy --force --clean` (only if `vars.PAGES_DEPLOY == 'true'`)

### `plans/progress.md`
- [x] Phase L (R342–R345) section added
- [x] Phase M (R346–R350) section added with per-task summaries
- [x] Phase M Summary paragraph written

---

## Coding Standards Verified
- [x] All shell scripts use `#!/usr/bin/env bash` + `set -euo pipefail`
- [x] Workflow YAML uses `actions/checkout@v4` and `actions/setup-python@v5`
- [x] Auto-commit uses `github-actions[bot]` identity
- [x] `[skip ci]` tag prevents infinite workflow loops
- [x] `PAGES_DEPLOY` is a repository *variable* (not secret) — safe to read in `if:`

---

## Status: ✅ COMPLETE
