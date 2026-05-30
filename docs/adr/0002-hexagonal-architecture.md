# ADR 0002: Adopt Hexagonal (Ports-and-Adapters) Architecture

**Date**: 2026-05-30  
**Status**: Accepted  
**Deciders**: PvPGN Core Team

## Context

The legacy PvPGN codebase (`src/bnetd/`) was a monolithic C application
ported to C++ without architectural discipline.  Key problems:

- Business logic (account management, game matching, ladder scoring) was
  entangled with I/O, SQL queries, and protocol parsing in the same
  translation units.
- Unit testing was nearly impossible: any test that touched account logic
  also required a live database connection and a running event loop.
- Adding a new storage backend (e.g., PostgreSQL) required forking large
  files and duplicating logic.
- The protocol layer directly called into storage functions, creating
  circular dependencies that made incremental refactoring dangerous.
- `src/bnetd/handle_bnet_link.cpp` grew to 3,500+ LOC; similar files
  exceeded 2,500 LOC.

The team evaluated several architectural patterns:
- **Layered (N-tier)**: simple but still allows upward dependencies.
- **Clean Architecture**: similar to hexagonal but with more prescribed
  layer names.
- **Hexagonal / Ports-and-Adapters**: explicit separation of domain,
  application, and infrastructure via interface ports.

## Decision

Adopt **hexagonal architecture** (also known as ports-and-adapters) as the
target structure for all new v3 code.

The source tree is organized as:

```
src/
├── domain/          # Pure business logic; no I/O, no framework deps
│   └── <context>/
│       ├── aggregates/
│       ├── value_objects/
│       ├── events/
│       ├── ports/       # Abstract interfaces (pure virtual C++ classes)
│       └── errors/
├── application/     # Use cases; orchestrates domain objects via ports
│   └── <context>/
├── infra/           # Adapters: SQL, file I/O, HTTP, Lua scripting
├── protocol/        # Network protocol codecs (bnet, irc, wol, telnet)
├── integration/     # Strangler-fig bridges to legacy bnetd code
└── app/             # Composition roots (main() for each binary)
```

Layering rules (enforced by `scripts/check_domain_purity.sh` and
`cmake/layering_exceptions.txt`):

- `domain/` must not `#include` anything from `infra/`, `protocol/`, or
  `integration/`.
- `application/` may depend on `domain/` ports but not on concrete
  `infra/` types.
- `infra/` implements `domain/` ports; it may depend on `domain/` and
  `application/` but not vice-versa.
- `protocol/` depends only on `domain/` value objects and standard library.

## Consequences

**Positive:**
- Domain and application layers are fully unit-testable without a database
  or network.
- New storage backends (PostgreSQL, Redis) can be added by implementing a
  port interface without touching domain logic.
- The layering check script (`scripts/check_domain_purity.sh`) catches
  accidental cross-layer dependencies in CI.
- Large files are naturally decomposed: each bounded context lives in its
  own directory.

**Negative / Trade-offs:**
- More boilerplate: every new feature requires at minimum a port interface,
  a domain aggregate, and an infra adapter.
- The strangler-fig `integration/` layer is a necessary but temporary
  violation of the hexagonal ideal; it will be removed when the legacy
  bnetd code is fully retired.
- `cmake/layering_exceptions.txt` tracks known violations that have not
  yet been cleaned up; it must shrink to zero before PvPGN 4.0.

**Related decisions:**
- ADR 0004 (Strangler Fig Pattern) describes how legacy code is migrated
  incrementally into this architecture.
