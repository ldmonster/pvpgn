# R344 — Layering CI Check

## Status: COMPLETE

## What was done

### New file: `.github/workflows/v3-layering.yml`
- **Triggers**: push/PR to `main` and `v3/**` branches
- **Job `layering-check`**:
  - Checks out repo
  - Runs `bash scripts/v3_layering_check.sh --report-file layering-report.txt`
  - Fails the build automatically if the script exits non-zero (exit 1 = violations, exit 2 = bad args)
  - Uploads `layering-report.txt` as a 30-day artifact on failure (conditional on `steps.layering.outcome == 'failure'`)

### Modified file: `scripts/v3_layering_check.sh`
- Added `--report-file <path>` argument parsing (POSIX `while/case` loop)
  - Writes all violation/allowed/summary lines to the file in addition to stdout
  - Truncates the file at startup (`:> "$REPORT_FILE"`)
  - Exits 2 if `--report-file` is given without a path argument
- Added `emit()` helper function that writes to both stdout and the report file
- Exit codes formalised:
  - `0` — no violations found
  - `1` — one or more violations found
  - `2` — bad arguments or `SRC_ROOT` directory not found
- Added summary line at the end: `"Layering check: N violations found"` (always printed, even on success)
- Preserved all existing `check_layer` logic and allow-list mechanism unchanged
- `set -e` retained; `total` normalised to `0` when the count file is empty

## Design decisions
- `emit()` helper avoids duplicating every `echo` call for stdout vs file output
- Report file is always written (even on success) so CI can archive it as a reference
- `--report-file` is optional — the script remains fully usable without it (backward compatible)
- Exit code 2 reserved for infrastructure errors (missing dir, bad args) to distinguish from logical violations (exit 1)
