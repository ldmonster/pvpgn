# Bug Hunt — original pvpgn-server vs v3 implementation

Comparing the upstream reference at `/home/cnupt/work/pvpgn-server` (PvPGN-PRO,
`src/bnetd` et al.) against this repo's v3 rewrite (`src/{domain,application,
infra,protocol}`), to find **behavioral divergences that are bugs** — places
where the v3 intends to replicate original behavior but gets it wrong (wrong
constant, byte order, formula, off-by-one, inverted condition, wrong default,
missing case). Intentional redesigns are NOT bugs.

## Method
1. Discovery fleet: one agent per subsystem, reads BOTH sides, writes findings to
   `findings/<subsystem>.md`. Each finding: severity, original ref (file:line),
   v3 ref (file:line), the divergence, and a proposed fix.
2. Triage (orchestrator): confirm real bugs vs false positives / intentional.
3. Fix fleet: apply confirmed fixes + regression tests; build + test green.

## Severity
- **CRIT**: breaks a client flow / data loss / crash / security.
- **HIGH**: wrong behavior a user would hit (wrong reply, wrong calc).
- **MED**: edge-case wrong behavior.
- **LOW**: cosmetic / unreachable / minor.

## Status index
See `STATUS.md` for per-subsystem progress and the confirmed-bug list.
