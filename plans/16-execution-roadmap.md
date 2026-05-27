# 16 — Execution Roadmap

**Goal:** Sequence all sub-plans into a single, dependency-respecting
order with checkpoints. Each row is one round (one PR with a
`plans/r###-checklist.md` file, building on the R209 template).

Round numbers are **indicative** — slip is allowed, but ordering is
not. Don't start a round whose prerequisites are red.

## 0. Where we are (snapshot 2026-05-27)

- R209 ✓ — `handle_bnet.cpp` (7.3 k LOC) relocated.
- Layered v3 tree present, hexagonal-ish but layering rules not yet
  CI-enforced.
- ~80 legacy files remaining in `src/bnetd/`, plus all of
  `src/d2cs/` and `src/d2dbs/`.

## 1. Phase 1 — Foundations (R210–R215)

Theme: Modern baseline before the big sweeps.

- **R210** C++20 bump *(01)*
- **R211** `core::format` → `std::format` *(01)*
- **R212** `Bytes` / `std::span` migration *(01)*
- **R213** Header self-contained tests *(01)*
- **R214** `[[nodiscard]]` audit *(01)*
- **R215** `enum class` sweep in `domain` + `application` *(01)*

**Checkpoint A:** v3 tree compiles `-Wall -Wextra -Wpedantic -Werror`
under C++20 on the full CI matrix.

## 2. Phase 2 — Strangler-fig completion (R216–R228)

Theme: empty `src/bnetd/`. See `02-finish-strangler-fig.md`.

- R216 `command.cpp`
- R217 `account*.cpp` + `attr*.cpp`
- R218 `channel*.cpp`
- R219 `game*.cpp` + `anongame*.cpp`
- R220 `clan/team/tournament/ladder`
- R221 `friends/mail/news/topic/userlog/watch`
- R222 `storage*` + `sql_*` **replaced** by `infra/persistence` *(07)*
- R223 `prefs.cpp` retirement *(09)*
- R224 remaining `handle_*`
- R225 utility sweep
- R226 `lua*` relocation *(13)*
- R227 `server/connection`
- R228 `main.cpp` → `app/bnetd/main.cpp`

**Checkpoint B:** `src/bnetd/` contains only headers retained for
external API. Run R201 + R209 style compose smoke after each round;
revert immediately on any regression.

## 3. Phase 3 — d2cs & d2dbs (R229–R232)

Same pattern as Phase 2, scaled down.

- R229 d2cs relocation pass 1
- R230 d2cs relocation pass 2 + d2cs main → `app/d2cs/`
- R231 d2dbs relocation pass 1
- R232 d2dbs relocation pass 2 + d2dbs main → `app/d2dbs/`

**Checkpoint C:** all three daemons live behind v3 composition roots.

## 4. Phase 4 — Domain & application formalisation (R233–R249)

- **R233** Publish canonical port headers *(05)*
- R234–R245 One bounded context per round — extract use cases from
  `integration_legacy_*` into `application/<bc>/*.cpp` *(04)*
- R246 Flip protocol handlers to call use cases directly *(04)*
- R247 In-memory fakes for every port *(05)*
- R248 CI layering check *(05, 12)*
- R249 Single composition root per daemon *(05)*

**Checkpoint D:** `application/` is the only place use cases live;
domain purity script passes.

## 5. Phase 5 — Cross-cutting concerns (R250–R295)

Parallelisable within phase (separate maintainers can own different
threads):

### Thread P5a — Protocols & fuzz (R250–R261)
Covers `06-protocol-and-codecs.md`. Rounds R250–R261.

### Thread P5b — Persistence (R262–R269)
Covers `07-persistence-and-migrations.md`.

### Thread P5c — Errors / logging (R270–R274)
Covers `08-error-handling-and-logging.md`.

### Thread P5d — Config / secrets (R275–R280)
Covers `09-config-and-secrets.md`.

### Thread P5e — Observability (R281–R286)
Covers `10-observability.md`.

### Thread P5f — Testing & CI (R287–R295)
Covers `11-testing-strategy.md` + `12-build-tooling-ci.md`.

**Checkpoint E:** all `12-build-tooling-ci.md` matrix axes green;
fuzz nightly stable for two weeks; coverage gate at target.

## 6. Phase 6 — Extension surfaces (R296–R302)

Covers `13-plugin-and-scripting.md`. Rounds R296–R302.

**Checkpoint F:** plugin ABI 1.0 frozen. Lua API v2 GA.

## 7. Phase 7 — Cutover & deletion (R303–R310)

Covers `14-legacy-retirement.md` + `15-release-and-versioning.md`.

- R303 `PVPGN_V3_BNETD_INTEGRATION` mandatory
- R304 `PVPGN_BUILD_LEGACY=OFF` default
- R305 Cut `v4.0`
- **(soak window — 1 release cycle)**
- R306 Delete legacy sources; rename `src/v3` → `src`
- R307 `SECURITY.md`, `RELEASING.md`
- R308 Changelog automation
- R309 SBOM
- R310 Signed artefacts; cut `v5.0`

## 8. Definition of Done (per round)

Every round PR must include:

1. `plans/r###-checklist.md` — diff stats, smoke result, decisions.
2. New / updated unit tests for any new code.
3. CI matrix green (Linux gcc + clang ASan/UBSan, macOS, MSVC).
4. v3 compose smoke green.
5. No new warnings under the applicable `-Werror` set.
6. Updated changelog entry under the upcoming version.

## 9. Definition of Done (overall)

- Layering CI check green.
- `src/bnetd|d2cs|d2dbs` deleted.
- `PVPGN_BUILD_LEGACY` deleted.
- Test coverage at target (see `11-testing-strategy.md`).
- Plugin ABI documented and shipped at 1.0.
- Lua API v2 shipped, v1 removed.
- All sub-plan documents marked `Status: COMPLETE`.

## 10. Cross-reference matrix

| Sub-plan | Prerequisite | Unblocks |
|----------|--------------|----------|
| 01 modern C++ | — | all others (warnings, std types) |
| 02 strangler-fig | 01 | 04, 05, 14 |
| 03 domain purity | 01 | 04 |
| 04 use cases | 03, 05 | 06 dispatcher rewiring, 13 |
| 05 ports | 03 | 04, 07, 13 |
| 06 protocol | 01, 05 | 11 fuzz, 14 |
| 07 persistence | 05 | 02 (R222), 14 |
| 08 errors/logging | 01 | 10, 11 |
| 09 config | 01 | 02 (R223), 14 |
| 10 observability | 05, 08 | 12 ops gates |
| 11 testing | 01 | 12 |
| 12 build/CI | 01 | gating everything |
| 13 plugins | 04, 05, 14 phase B | 14 |
| 14 retirement | 02, 04, 07, 13 | 15 |
| 15 release | 12 | — |
