# 12 — Build System & Dependencies

A maintainable build is part of a maintainable codebase. The goals: the
dependency rule is mirrored in CMake link edges, optional backends degrade
gracefully, and a newcomer can build and test with one preset.

## 1. Target granularity

- One CMake target per code-bearing leaf directory (a static library), with an
  explicit `target_link_libraries` that names **only** the layers the
  dependency rule permits. The link graph is a second, compiler-enforced copy of
  the architecture diagram in [03-target-architecture.md](03-target-architecture.md).
- A small `check` (or a CMake `BUILD_INTERFACE` audit) asserts no target links a
  forbidden layer — catching violations the header scanner might miss.
- Public vs private headers: published API under `include/`, internals beside
  the `.cpp`. `core` links nothing v3 and is independently buildable.

## 2. Presets (already established, keep coherent)

`CMakePresets.json` defines `v3-dev` (Debug+tests), `v3-release` (LTO +
warnings-as-errors), `v3-asan/ubsan/tsan`, `v3-coverage`. Keep these as the
canonical entry points; everything in [11-local-quality-gates.md](11-local-quality-gates.md)
drives off them. Add CTest preset labels so `ctest --preset v3-dev` runs the
right band.

## 3. Optional backends & runtimes (graceful degradation)

The local box may lack `sqlite3`, libsodium, Lua, MySQL/Postgres, Docker. The
build must:

- **Detect** each dependency (`ConfigureChecks.cmake`) and set a feature flag.
- **Compile the core regardless** — the in-memory backend and the no-op
  observability adapter are always available, so unit/functional/e2e (combined
  service over SQLite-or-inmemory) always run.
- **Skip, not fail,** when an optional backend is absent; the corresponding
  contract/integration tests register as skipped.
- Keep the **port** present even when its only adapter is unavailable — code
  compiles against the interface; only the adapter target is gated.

## 4. Dependency management (KISS + YAGNI)

- Prefer the **standard library** over vendored code: `std::` containers,
  strings, `std::expected`, `std::chrono` replace hand-rolled `hashtable`,
  `xstring`, `fdwatch`, bespoke time.
- Vendor only **vetted, necessary** libraries (crypto via libsodium, TOML
  parser, an async runtime, test framework, fuzz engine). `vcpkg.json` pins
  them; `vendor/` holds anything not in vcpkg.
- Do not add a dependency for something the stdlib does. Do not pre-add a
  dependency for a backend we don't yet build (YAGNI).

## 5. Toolchain floor

- Document the minimum compiler (currently GCC 13 / Clang) and the C++ standard
  (C++23 with fallback shims for `std::flat_map`/`std::print` where libstdc++ 13
  lacks them).
- Provide a single switch to flip shims → literal stdlib spellings once the
  floor rises; guard it so older toolchains still build.
- `v3-release` builds with warnings-as-errors so the floor stays clean.

## 6. Reproducible local container (optional, no CI)

- `Dockerfile`/`Dockerfile.distroless` and `docker-compose.*.yml` exist for the
  developer who *wants* a hermetic build/integration environment locally. They
  are **developer conveniences**, not a pipeline. Multi-arch/signing/SBOM
  scaffolds are kept as docs/ADRs until a real release host exists; they are not
  wired to any CI.

## 7. Tasks for this plan

1. Audit every `target_link_libraries` against the dependency table; remove
   forbidden edges; add the link-edge `check`.
2. Ensure `ConfigureChecks.cmake` gates every optional backend and the build is
   green with **none** of them present (in-memory path).
3. Replace remaining hand-rolled primitives with stdlib; delete the bespoke
   types and their tests.
4. Trim `vcpkg.json`/`vendor/` to actually-used, justified dependencies.
5. Document the toolchain floor and the shim→literal switch in `docs/`.

## Definition of Done

- [ ] No CMake target links a layer forbidden by the dependency rule (link-edge
      check green).
- [ ] A clean checkout with no optional backends builds and runs unit +
      functional + (in-memory/sqlite) e2e green.
- [ ] No hand-rolled `hashtable`/`xstring`/`fdwatch` remain; stdlib used
      throughout.
- [ ] `vcpkg.json` lists only dependencies with a real consumer.
