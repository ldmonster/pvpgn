# Phase 3 plan — Legacy handler retirement

> Scoping doc, written 2026-05 (R166). Translates the "delete legacy
> handlers" intent into a concrete, round-sized backlog. No code yet.

## Goal

Migrate every legacy `handle_*` and `_handle_*_command` function from
`src/bnetd/` (and `src/d2cs/`, `src/d2dbs/`) onto the v3 FSM /
command-bus architecture so the legacy translation units can be
deleted entirely. End state: `bnetd_legacy`, `d2cs_legacy`, and
`d2dbs_legacy` static libraries are empty and removed.

## Constraints from prior rounds

- Strangler-fig pattern: every legacy entry point already has an
  `extern "C" int pvpgn_v3_<op>_try(void*, ...)` hook (R142-R150).
- `pvpgn_v3_bnetd` already runs as a parallel daemon (Boost.Asio +
  `LegacyBridge::tick()`). It links nothing from `bnetd_legacy`.
- The legacy `bnetd` binary is still operationally relevant -- the
  v3 daemon does not yet own packet I/O, account storage, or game
  lifecycle.

## Inventory (rough)

| Subsystem                  | Legacy file(s)                                    | v3 status      | Round target |
| -------------------------- | ------------------------------------------------- | -------------- | ------------ |
| Init / auth                | `handle_init.cpp`, `handle_auth.cpp`              | partial        | Phase 3.A    |
| Bnet chat / channel        | `handle_bnet.cpp`, `channel.cpp`                  | partial        | Phase 3.B    |
| Friend list                | `handle_bnet.cpp` (friend_*)                      | dispatch ready | Phase 3.C    |
| Anongame                   | `handle_anongame.cpp`, `anongame_*.cpp`           | snapshots only | Phase 3.D    |
| Clan                       | `clan.cpp`, `command.cpp` (clan_*)                | none           | Phase 3.E    |
| Tournament / ladder        | `tournament.cpp`, `ladder*.cpp`                   | none           | Phase 3.F    |
| Telnet admin               | `handle_telnet.cpp`                               | FSM stub       | Phase 3.G    |
| IRC                        | `irc.cpp`, `handle_irc.cpp`                       | v3 FSM         | Phase 3.H    |
| Apireg / WOL               | `handle_apireg.cpp`, `handle_wol.cpp`             | none           | Phase 3.I    |
| D2CS dispatch              | `src/d2cs/handle_d2*.cpp`                         | none           | Phase 4.A    |
| D2DBS storage              | `src/d2dbs/dbserver.cpp`, `dbspacket.cpp`         | none           | Phase 4.B    |

## Recommended first cut (Phase 3.A: auth + init)

1. Audit every entry point in `src/bnetd/handle_init.cpp` and
   `handle_auth.cpp`; map each packet -> existing v3 handler in
   `src/v3/application/auth/`.
2. For each gap, add a v3 `IAuthHandler` (or extend the existing
   `LoginCommand`/`AuthInfoCommand` aggregates) that consumes the
   protocol DTOs already produced by `protocol_bnet`.
3. Replace each legacy `handle_*` body with a call to the
   `pvpgn_v3_*_try` bridge; on `0` returned, skip the legacy fall-
   through.
4. When ALL hooks in a TU return `0` reliably, delete the legacy
   TU. Track deletions in this file.

## Acceptance gate per round

- Catch2 v3 suite green (1225+/213+ assertions).
- Manual smoke: a Diablo II / StarCraft client connects, logs in,
  joins a channel, sees the channel list. (Or the equivalent
  unit-level harness once available.)

## Out of scope

- Storage migration (still file/sql legacy driver layer).
- D2GS protocol (third-party, never owned by pvpgn).
