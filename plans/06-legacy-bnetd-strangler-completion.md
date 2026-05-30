# 06 — Finish the legacy-bnetd strangler

## What

`src/integration/legacy_bnetd/` exists as a temporary bridge: each legacy handler calls `pvpgn_v3_<op>_try` first, falls back to legacy on failure. The goal of this plan is to drive every bridge to **100 % v3 coverage**, then delete the legacy implementation and the bridge.

## Status quo (from `src/integration/legacy_bnetd/src/`)

~170 `*_bridge.cpp` files, three lifecycle revisions (`r246`, `r247`), three `_link.cpp` glue files. Each bridge has a known coverage state — track in `refactoring-progress.md`:

```
| Bridge                     | v3 coverage | Legacy callers | Notes |
| -------------------------- | ----------- | -------------- | ----- |
| account_dispatch_bridge    | ?           | bnetd          |       |
| ads_bridge                 | 100 %       | bnetd          | ready to flip |
| auth_dispatch_bridge       | ?           | bnetd          |       |
...
```

## Concrete steps

For each bridge, in dependency order:

1. **Audit** — `grep` for `pvpgn_v3_*_try` return-value branches in the bridge. Identify the legacy fallback path.
2. **Cover** — write a v3 application service that satisfies every input the legacy fallback handled. Unit-test it (`tests/unit/application/<ctx>/`).
3. **Soak** — flip the bridge into "shadow mode": call both, compare results, log mismatches (`src/infra/shadow/`).
4. **Promote** — remove the legacy fallback; bridge becomes a one-line forward.
5. **Inline** — once the bridge is a one-liner, inline the call at the caller and delete the bridge file.
6. **Delete legacy** — once **all** bridges in a sub-system are inlined, delete the corresponding `src/<legacy>/bnetd/...` files (legacy tree).

## Lifecycle bridge dedup

`bnetd_lifecycle_bridges.cpp` + `_r246.cpp` + `_r247.cpp` are three revisions of the same logic. Collapse to one file once the `r247` path is the only live one (it is, in 3.0.0).

## Strangler invariants (lift from user memory)

- Bridges return 0 on any failure so legacy fallback runs. **After promotion**, the bridge returns the real result and there is no fallback.
- `bnetd_legacy` headers (with `setup_before.h` / `setup_after.h`) may only be included from `src/integration/legacy_bnetd/`.
- `application` MUST NOT depend on `infra`. Verified by `scripts/v3_layering_check.sh`.

## Acceptance criteria

- [ ] `src/integration/legacy_bnetd/` shrinks by ≥ 80 % LOC.
- [ ] `src/bnetd/` (legacy tree, currently invoked only via bridges) is deleted.
- [ ] `PVPGN_BUILD_LEGACY` cmake option is removed; the only remaining build is v3.
- [ ] `bnetd-v3` is renamed `bnetd`.
- [ ] No `pvpgn_v3_*_try` symbol remains — direct v3 calls everywhere.

## Risks

- A bridge promoted before its v3 service is feature-complete causes a behaviour regression. **Mitigation**: shadow-mode is mandatory for 7 days before promotion in production; record diff counts in metrics.

## Out of scope

- Adding new functionality during strangler work. Only behaviour-preserving moves.
