# 00 — Vision and Scope

## 1. What PvPGN is

PvPGN is a server emulator for the classic Battle.net protocol family: it speaks
`bnet` (Diablo/StarCraft/Warcraft III), `d2cs`/`d2dbs`/`d2gs` (Diablo II realm,
database, game servers), plus `irc`, `telnet`, `wol` (Westwood Online) and a
handful of file/UDP side-channels. Historically it was a C codebase with three
daemons (`bnetd`, `d2cs`, `d2dbs`) sharing a large `common/` grab-bag, global
mutable state, hand-rolled `fdwatch`/`hashtable`/`xstring`, and bespoke storage
and crypto.

The codebase has already been moved a long way toward a layered, hexagonal C++
design (see [02-current-state.md](02-current-state.md)). This plan defines the
*finished* shape and the local discipline needed to reach and hold it.

## 2. Why refactor (the problems we are solving)

- **Coupling & global state.** Logic, I/O, storage, and protocol parsing were
  historically interleaved, making units impossible to test in isolation.
- **Duplication across daemons.** `bnetd`/`d2cs`/`d2dbs` re-implemented the same
  primitives. DRY violations multiplied bug surface.
- **Hidden requirements in I/O.** Business rules lived inside socket and SQL
  callbacks, so behaviour could only be exercised against a live server.
- **Hard to extend.** Adding a storage backend, a protocol revision, or a new
  command meant editing many files in lockstep (Open/Closed violation).
- **Thin, brittle tests.** Coverage concentrated on a few leaf utilities;
  protocol and end-to-end behaviour were verified manually.

## 3. The vision

> A small, pure **domain** of Battle.net game-service rules; a thin
> **application** layer of single-purpose use-cases that orchestrate the domain
> through **ports**; **infrastructure** and **protocol** adapters that are the
> only places allowed to touch sockets, databases, crypto libraries, files, and
> clocks; and **composition roots** (`services/`, `app/`) that wire concrete
> adapters to ports. Every rule is unit-testable without I/O; every protocol is
> fuzz- and golden-tested; every supported client journey has an automated
> end-to-end test; and every architectural invariant is enforced by a script a
> developer can run before committing.

## 4. Success criteria (measurable, local)

A reader can confirm each of these on their own machine:

1. `scripts/v3_layering_check.sh src` exits 0 with an **empty** allow-list.
2. `scripts/check_domain_purity.sh src/domain` exits 0 — the domain contains no
   I/O, no logging library, no clock calls, no global mutable state.
3. `ctest` from a `v3-dev` build is 100% green; the unit suite covers every
   domain aggregate and every application use-case (`check-unit-pairing.sh`
   finds no unpaired source).
4. Every protocol family under `src/protocol/` has a fuzz target and a
   golden-vector test; `tests/fuzz` builds and runs a smoke corpus locally.
5. Every shipped client journey (login, chat, channel, file transfer, stats,
   game create/join, ladder) has an `tests/e2e` scenario that runs against a
   locally-spawned server with a scripted fake client.
6. Line coverage of `domain/` + `application/` is ≥ 85% locally
   (`check-coverage.sh`), and a mutation pilot on `domain/identity` kills
   ≥ 80% of mutants.
7. `mkdocs build --strict` is green; config, plugin, and Lua reference docs are
   generated from source, not hand-maintained.

## 5. Non-goals (YAGNI)

- **No CI service.** All gates are local. Do not author `.github/`/`.gitlab-ci`
  pipelines as part of this plan.
- **No new gameplay features.** This is structural; behaviour is preserved and
  pinned by tests, not changed.
- **No protocol extensions.** We match existing client expectations exactly.
- **No speculative backends or ports.** Abstractions appear only when a second
  real consumer exists.
- **No rewrite-from-zero.** We finish the strangler-fig migration already in
  flight; we do not restart it.

## 6. Definition of Done for this section

- [ ] Team agrees the success criteria in §4 are the contract for "refactor
      complete."
- [ ] Non-goals in §5 are accepted; scope creep is rejected against this list.
