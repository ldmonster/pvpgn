# 15 — Release and Rollout

## What

Codify the release process for the wave-two changes: semver
discipline, deprecation policy, rolling-upgrade procedure, distroless
container, signed release artefacts.

## Why

- Wave two breaks legacy ABIs (plug-ins), drops config keys, changes
  password storage, and replaces the I/O loop. Operators need a
  predictable upgrade story.
- Today, release notes are ad-hoc and there is no signed artefact
  pipeline.

## Concrete steps

1. **SemVer policy** in `docs/developer/release-process.md`:
   - Wire protocol changes ⇒ major.
   - Plugin ABI v1→v2 ⇒ major.
   - TOML schema breaking change ⇒ major.
   - New feature flag, opt-in ⇒ minor.
   - Bug fix, no schema change ⇒ patch.
2. **Deprecation policy.** Any removed config key / plugin hook /
   metric must be:
   - Announced in a minor release with a startup warning.
   - Removed no earlier than the next major release.
   - Documented in `CHANGELOG.md` under `### Deprecated`.
3. **Rolling-upgrade procedure** (`docs/operator/runbooks/rolling-upgrade.md`):
   - Pre-flight `bnetd --check-config` against new schema.
   - Backwards-compat window for at-rest hash (plan 08) and TOML
     schema versioning.
   - Health probe contract during restart.
4. **Distroless image** (`Dockerfile.distroless`):
   - `gcr.io/distroless/cc-debian12` base.
   - Multi-stage build; final image < 80 MB.
   - Runs as non-root by default.
5. **Multi-arch build.** Publish `linux/amd64` and `linux/arm64`
   images via `docker buildx` in CI release workflow.
6. **Signed artefacts.** Sign release binaries and container images
   with sigstore / cosign. Publish public key in repo and on the
   release page.
7. **SBOM.** Emit CycloneDX SBOM with every release artefact;
   attach to GitHub release.
8. **`CHANGELOG.md` discipline.** Enforce `Keep a Changelog` format;
   CI lints headings on PRs that touch `CHANGELOG.md`.

## Acceptance criteria

- [ ] `docs/developer/release-process.md` published; PR template
      references it.
- [ ] `docs/operator/runbooks/rolling-upgrade.md` walks operators
      through a no-downtime upgrade from `3.0.x` to wave-two release.
- [ ] Distroless image builds and runs the full integration test
      suite.
- [ ] Multi-arch tags published for the next release.
- [ ] Release artefacts signed; SBOM attached.
- [ ] `CHANGELOG.md` lints in CI.

## Risks

- Signed-artefact tooling adds a one-time learning cost. Pilot in a
  dry-run release before flipping the next real one.
- Distroless missing libc++ symbols (statically linked vs dynamic
  decision). Decide early; record in ADR `0011-runtime-image.md`.

## Out of scope

- Helm charts / Kubernetes operators (community-owned).
- Windows release artefacts beyond what we ship today.
