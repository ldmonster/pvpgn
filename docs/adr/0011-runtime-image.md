# ADR 0011: Distroless Runtime Image

**Date**: 2026-06-02
**Status**: Proposed
**Deciders**: PvPGN Core Team

## Context

Plan 15 (`plans/15-release-and-rollout.md`) asks for a small, hardened runtime
image: `gcr.io/distroless/cc-debian12` base, multi-stage build, final image
< 80 MB, runs as non-root. Today the only runtime stage is `Dockerfile.v3`'s
`v3-runtime`, which is **Alpine**-based (musl libc) and ships a shell + apk.

The plan's Risks note flags the core decision: **static vs dynamic linking** —
"distroless missing libc++ symbols". This ADR records that decision and the
constraints a working `Dockerfile.distroless` must satisfy.

## Decision

1. **Dynamic linking against the distroless-provided C/C++ runtime.** Do **not**
   statically link. `gcr.io/distroless/cc-debian12` ships glibc, `libgcc`, and
   `libstdc++` — exactly the C++ runtime our binary needs. A static build would
   bloat the image, complicate the libstdc++ license posture, and lose the
   security-update path for the base runtime. We dynamically link and copy only
   the **non-distroless** shared objects (Boost, Lua, SQLite, OpenSSL, libcurl,
   zlib, spdlog) next to the binary.

2. **glibc compatibility: build on the same Debian release as the runtime.**
   glibc is forward-compatible, not backward-compatible: a binary linked against
   glibc *N* will not run on glibc *< N*. `cc-debian12` is Debian 12 (bookworm,
   glibc 2.36), so the **builder must be `debian:12`-based** (glibc ≤ 2.36) —
   **not** Ubuntu 24.04 (glibc 2.39) or Alpine (musl). This is the single most
   common distroless failure and is non-negotiable.

3. **Toolchain: GCC ≥ 13 on the bookworm builder.** The tree is C++23 and the
   project floor is GCC 13 (see Plan 09). Debian 12's default is GCC 12, which is
   insufficient, so the builder installs a newer GCC (bookworm-backports or an
   equivalent toolchain) while keeping the glibc-2.36 base. The CLI tools that
   need `<print>` (libstdc++ 14) are not part of the runtime image — only
   `bnetd` is shipped, and it does not use `<print>`.

4. **Exact dependency copy via `ldd`.** Rather than hand-maintaining a shared-lib
   list, the build stage resolves the binary's needs with `ldd` and copies each
   resolved object (plus the dynamic loader) into a `/rootfs` staging tree, which
   is then `COPY`'d wholesale into the distroless stage. This keeps the image
   minimal and self-correcting as dependencies change.

5. **Non-root + minimal surface.** Use the `:nonroot` distroless tag and run as
   that user. No shell, no package manager. Config ships at
   `/etc/pvpgn/bnetd.toml`; data is expected on a mounted volume.

## Consequences

- **Positive**: tiny, CVE-light, non-root image; dynamic linking keeps it small
  and lets the base runtime receive security updates; the `ldd` copy is
  self-maintaining.
- **Negative**: the builder is pinned to the runtime's Debian release for glibc
  parity, and needs a non-default GCC on that base — a more constrained builder
  than the Alpine one. SQLite/MySQL/PostgreSQL client libs must be present in
  the builder and copied in if those backends are enabled at runtime.

## Status / validation

`Dockerfile.distroless` is committed as a **best-effort scaffold**: it encodes
the decisions above (debian-12 builder, `ldd`-driven copy, distroless
`:nonroot` runtime) but is **untested** in this environment (no Docker; the
GCC-on-bookworm + glibc-parity combination must be validated with an actual
`docker build`). The < 80 MB target and the precise GCC-install incantation are
first-run calibration points — confirm them in a dry-run release (Plan 15
Risks) before flipping the release pipeline to this image.
