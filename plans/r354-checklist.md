# R354 Checklist — Legacy retirement plan

## Changes
- [x] Created `scripts/dev/retire-legacy.sh` (dry-run + `--apply` mode, `chmod +x`)
- [x] Created `docs/legacy-retirement-plan.md` with full retirement process documentation
- [x] Updated `plans/progress.md` with Phase N completion entry

## Notes
- Actual deletion of legacy sources is deferred to PvPGN 4.0.0
- No existing source files were deleted in this round
- The retirement script is safe to run in dry-run mode at any time

## Result
Legacy retirement plan is documented. The strangler-fig migration is complete.
Actual source deletion is scheduled for PvPGN 4.0.0.
