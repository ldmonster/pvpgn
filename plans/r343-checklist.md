# R343 — .clang-tidy + CI Tidy Jobs

## Status: COMPLETE

## What was done

### New file: `.clang-tidy` (repo root)
- Enabled check groups: `clang-diagnostic-*`, `clang-analyzer-*`, `cppcoreguidelines-*`, `modernize-*`, `performance-*`, `readability-*`, `bugprone-*`
- Disabled noisy/inapplicable checks:
  - `-modernize-use-trailing-return-type` (conflicts with project style)
  - `-readability-magic-numbers` (too noisy for protocol code)
  - `-cppcoreguidelines-avoid-magic-numbers` (same)
  - `-cppcoreguidelines-pro-type-reinterpret-cast` (needed for wire-format codecs)
  - `-cppcoreguidelines-pro-bounds-pointer-arithmetic` (needed for legacy bridge code)
- `WarningsAsErrors: ''` — warnings are reported but do not fail the build by default (CI controls this via `--warnings-as-errors`)
- `HeaderFilterRegex: 'src/v3/.*'` — only diagnose headers inside the v3 sub-tree
- `FormatStyle: file` — respects `.clang-format` at the repo root
- Identifier naming conventions enforced:
  - Classes: `CamelCase`
  - Functions, variables, members: `lower_case`
  - Member suffix: `_` (trailing underscore)
  - Constants: `UPPER_CASE`
  - Namespaces: `lower_case`

### New file: `.github/workflows/v3-tidy.yml`
- **Triggers**: push/PR to `main` and `v3/**` branches; nightly schedule `0 2 * * *`
- **Job `tidy-full`** (schedule only):
  - Installs `clang-18`, `clang-tidy-18`, `cmake`, `ninja-build`, `libsqlite3-dev`, `libssl-dev`
  - Configures with `cmake --preset v3-dev -DCMAKE_EXPORT_COMPILE_COMMANDS=ON`
  - Runs `run-clang-tidy-18 -p build/v3-dev 'src/v3/.*\.cpp$'` — full scan of all v3 sources
- **Job `tidy-incremental`** (push/PR only):
  - Same install + configure steps
  - Uses `git diff --name-only $BASE...HEAD | grep '^src/v3/.*\.cpp$'` to find changed files
  - Runs `clang-tidy-18 -p build/v3-dev <changed files>` — only scans modified files
  - Skips gracefully if no v3 `.cpp` files changed

## Design decisions
- Two separate jobs (not one with a conditional) so GitHub Actions shows them as distinct status checks
- `fetch-depth: 0` on incremental job to ensure full git history for `git diff`
- `clang-18` pinned to match the `v3-asan` and `v3-tsan` presets
- `run-clang-tidy` (not `clang-tidy` directly) for full scan to enable parallel execution across TUs
