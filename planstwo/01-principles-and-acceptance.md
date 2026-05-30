# 01 — Principles and Acceptance Criteria

## Principles (wave two reinforces wave one)

1. **One canonical path.** No "legacy + v3" pair survives a wave-two
   merge. If a bridge is touched, the bridge is removed in the same PR
   or the PR is rejected.
2. **DDD bounded contexts own their seams.** A port lives next to the
   aggregate that needs it (`domain/<ctx>/ports/`), never in a global
   `application/ports/` bucket.
3. **KISS first.** No new abstraction with a single implementation.
   No "future-proof" hook without a caller landing in the same PR.
4. **DRY by deletion.** Duplicate implementations are collapsed by
   removing N-1 of them, not by extracting a shared base class.
5. **YAGNI.** No metric, log field, plugin hook, config key, or CLI
   flag without a documented consumer.
6. **Modern C++.** C++23 is the floor by the end of wave two. Prefer
   `std::expected`, `std::span`, `std::string_view`, `std::chrono`,
   `std::filesystem`, `std::format` / `std::print`, ranges, coroutines.
7. **No platform `#ifdef` outside `infra/`.** Anything portable lives
   in `core/`; anything OS-specific lives in `infra/<facet>/`.
8. **Tests are written with the change, not after.** A PR without
   matching unit tests under `tests/unit/<layer>/<ctx>/` is incomplete.

## Definition of Done (per plan file)

Every wave-two plan file is "done" when **all** of the following hold:

- All acceptance criteria in the file are ticked.
- `cmake --build --preset v3-release` is green on Linux + Windows.
- `ctest --preset v3-release` passes; new unit tests added for new code.
- `scripts/v3_layering_check.sh` passes with no new exceptions.
- `mkdocs build --strict` passes.
- Sanitizer matrix (ASan, UBSan, TSan) is green for affected targets.
- `refactoring-progress.md` updated under `## Wave Two` with the date,
  plan number, and PR / commit.
- Any removed file is removed from CMake, docs, scripts, and CI.
- Any new dependency is justified in an ADR under `docs/adr/`.

## Code review checklist (paste into PR template)

- [ ] No new `src/common/` file.
- [ ] No new `application/ports/` file (use `domain/<ctx>/ports/`).
- [ ] No new `pvpgn_v3_*_try` symbol; bridges are direct calls or gone.
- [ ] No new `#ifdef _WIN32` outside `infra/`.
- [ ] No new vendored library copy (use vcpkg).
- [ ] All `// TODO` comments reference a tracked issue.
- [ ] Public headers compile with `-Wall -Wextra -Wpedantic -Werror`.
- [ ] No raw `new` / `delete` outside RAII wrappers.
- [ ] No `printf` / `fprintf`; use `core::log` or `std::print`.

## Rollback policy

Every wave-two plan must declare a rollback strategy. The default is:
revert the merge commit. Plans that touch persistence (07), wire layout
(none planned), or rolling-upgrade contract (15) must document a
forward-compatible escape hatch.
