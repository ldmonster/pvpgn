# 00 — Overview

## Where we are

- `3.0.0` shipped a full v3 sub-tree under `src/{core,domain,application,infra,integration,protocol,app,services,runtime,scripting}` driven by `bnetd-v3`, `pvpgn-migrate`, `pvpgn-config`.
- The legacy tree still lives under `src/integration/legacy_bnetd/` (≈170 bridge translation units, biggest is `handle_bnet_link.cpp` at 6.1 kLOC) plus `src/common/` (vendored `pugixml`, hand-rolled `xalloc`, `hashtable`, `fdwatch`, …).
- v3 build is configured exclusively from TOML; legacy `.conf` parsers still exist in the legacy build path but are no longer installed under v3.
- A `lua/` tree at the repo root is still copy-installed verbatim to `${LOCALSTATEDIR}/lua`.
- `build/{legacy,linked,v3,dev-release}/` artefacts live in the workspace; not in git, but referenced by docs and scripts inconsistently.

## What "more solid / DDD / KISS / DRY / YAGNI / modern" means here

1. **Solid** = one canonical code path (v3) per feature; no parallel "legacy + v3" implementations once a bridge is retired.
2. **DDD** = each bounded context (Identity, Chat, Realm, Ladder, Matchmaking, Tournament, Moderation, Game, Ads, Connection) owns its domain types, application services, ports, and adapters — no cross-context reach-through.
3. **KISS** = remove ceremony: no abstractions with one implementation, no "future-proof" hooks without a caller, no string-keyed dispatcher tables when a `switch` suffices.
4. **DRY** = collapse duplicated parsing (`bnetd.conf` vs `bnetd.toml`), duplicated lifecycle bridges (`bnetd_lifecycle_bridges{,_r246,_r247}.cpp`), duplicated FSMs.
5. **YAGNI** = drop ad-banner formats nobody serves, perl converters for CVS, init scripts for distros that no longer exist, `tos.bat`, mock-only ABI surface area.
6. **Modern** = C++20 floor is in (`CMakeLists.txt` line 24); next step is to lean on `<expected>`, `<span>`, `<chrono>`, `<filesystem>`, coroutines for async I/O, modules where the toolchain supports them.

## What "expandable / testable" means here

- New protocol / new game / new persistence backend = one folder under `domain/<ctx>`, one under `application/<feature>`, one adapter under `infra/<tech>`, one binary wire-up.
- "Testable" = every domain + application unit testable with **no** linkage to legacy `bnetd_legacy`, no SQLite/MySQL/network fixtures, sub-second per test.

## Non-goals

- Rewriting the wire protocols. Battle.net packet IDs / WOL framing / IRC dialect are immutable.
- Replacing Lua with another scripting language.
- Adding a web UI beyond the existing `webui` health/metrics surface.
- Supporting clients newer than what `3.0.0` already supports.
