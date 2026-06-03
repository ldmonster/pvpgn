# 03 — Target Architecture

## 1. The layered/hexagonal model

```
                       ┌───────────────────────────────────────┐
                       │  app/        (entrypoints / mains)     │
                       │  services/   (composition roots / DI)  │
                       └───────────────┬───────────────────────┘
                                       │ wires concrete adapters to ports
        ┌──────────────────────────────┴──────────────────────────────┐
        │ integration/   (cross-protocol orchestration, legacy seams)  │
        ├───────────────────────────────────────────────────────────── ┤
        │ infra/         (DB, net, crypto, config, log, metrics, lua)   │  ADAPTERS
        │ protocol/      (wire codecs & framing per family)             │  (driven side)
        ├───────────────────────────────────────────────────────────── ┤
        │ application/   (use-cases; orchestrate domain via ports)      │  PORTS
        ├───────────────────────────────────────────────────────────── ┤
        │ domain/        (aggregates, invariants, events, ports)        │  CORE
        ├───────────────────────────────────────────────────────────── ┤
        │ core/          (pure utilities; no v3 deps)                   │  KERNEL
        └───────────────────────────────────────────────────────────── ┘
```

## 2. The dependency rule (the one invariant that matters)

Each layer may `#include` only from itself or layers **above it in purity**
(below it in the diagram):

| Layer | May depend on |
|-------|---------------|
| `core` | nothing v3 |
| `domain` | `core` |
| `application` | `core`, `domain` |
| `protocol` | `core`, `domain` |
| `infra` | `core`, `domain`, `application`, `protocol` |
| `integration` | all of the above |
| `services` / `app` | everything |

**Hard rule:** `application` MUST NOT depend on `infra`. The arrow always points
inward toward the domain. This is mechanically enforced by
`scripts/v3_layering_check.sh`; the allow-list is the list of known violations
still to retire, and it may only shrink.

## 3. Ports & adapters

- **Ports** are abstract interfaces *owned by the inner layer that needs the
  capability*: domain ports in `domain/<ctx>/ports.hpp`, application ports in
  `application/<ctx>/.../ports`.
- **Adapters** are concrete implementations in `infra/` or `protocol/`.
- **Composition roots** (`services/<daemon>`, `app/<daemon>`) are the *only*
  place that names concrete adapters and binds them to ports. There is no
  service locator and no global registry reached from business code; wiring is
  explicit constructor injection.

This is what makes the domain and application layers unit-testable with fakes
and keeps backends swappable (Open/Closed + Dependency-Inversion).

## 4. Module/target shape in CMake

- Every leaf directory with code is its own CMake target (static lib) with an
  explicit `target_link_libraries` declaring its allowed dependencies. CMake
  link edges are a *second* enforcement of the dependency rule, complementing
  the include-scanner.
- Public headers live under `include/<layer>/<module>/…`; private headers under
  `src/<…>/`. The split makes the published surface obvious.
- `core/` builds with **zero** v3 dependencies and could be extracted as a
  standalone library — a good litmus test for purity.

## 5. The three daemons and the combined service

- `bnetd`, `d2cs`, `d2dbs` remain as deployable units (`app/<daemon>` mains,
  `services/<daemon>` roots) for compatibility.
- `services/combined` allows running them in one process for dev/e2e.
- All four share the *same* domain/application/infra code — the historical
  per-daemon duplication is gone. Differences are *only* in which protocol
  adapters and ports each composition root wires up.

## 6. Extension points (and the YAGNI guard around them)

The architecture supports adding, **without touching core logic**:

- a **storage backend** → new `infra/<backend>/` implementing `IDbDriver`,
  registered in a composition root;
- a **protocol revision / new command** → new codec + handler registered in the
  family's dispatch registry;
- an **auth scheme** → new adapter behind the auth port + an upgrade policy;
- a **scripting hook / plugin** → published C ABI + capability descriptor.

Each of these is a real, existing seam. **No further extension points are added
speculatively** — a new port appears only when a second concrete consumer does.

## 7. Naming & boundaries conventions

- Namespaces mirror directories: `pvpgn::domain::identity`,
  `pvpgn::application::auth`, `pvpgn::infra::sqlite`, etc.
- Wire/legacy structs never cross inward past `protocol`/`infra`; they are
  translated to domain types at the boundary (anti-corruption layer).
- Cross-context communication is via domain events or explicit application
  orchestration — never by one context including another's internals.

## Definition of Done

- [ ] `scripts/v3_layering_check.sh src` passes with an empty allow-list.
- [ ] CMake link edges match the table in §2 (a `check` confirms no target links
      a disallowed layer).
- [ ] `core/` builds and unit-tests with no dependency on any other `src/` layer.
- [ ] No service-locator/global-registry access exists in `domain`/`application`
      (composition roots do all wiring).
