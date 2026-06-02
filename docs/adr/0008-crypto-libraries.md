# ADR 0008: Crypto Libraries and Password-at-Rest Modernization

**Date**: 2026-06-02
**Status**: Accepted
**Deciders**: PvPGN Core Team

## Context

Plan 08 (`plans/08-crypto-modernization.md`) replaces the bespoke at-rest
password hashing and consolidates randomness onto one canonical source, while
preserving wire-format compatibility for shipped game clients.

Starting state when this ADR was written:

- **At-rest password hash** is `bnethash` (a Blizzard SHA-1 derivative). It
  stores a password-equivalent: once the digest leaks, the account is
  compromised — there is no per-user salt and no work factor.
- **Wire SRP** for WAR3/W3XP NLS login is the hand-rolled `bnetsrp3`
  (SRP-6a, 1024-bit). A clean C++ reimplementation already lives in
  `src/infra/crypto/bnet_srp3.{hpp,cpp}` (plus `nls.*`); the legacy copy in
  `src/common/bnetsrp3.*` is unused by the v3 build.
- **WoL hashing** is `wolhash`; a v3 reimplementation exists in
  `src/infra/crypto/wol_hash.*`.
- **Randomness**: the only `std::rand()` call left in `src/` is inside the
  legacy `src/common/bigint.cpp` (a Plan-08 file, not compiled by the v3
  build). The v3 tree already exposes an `application::ports::IRandomSource`
  port (`domain/shared/ports/random_source.hpp`) with an in-memory test
  adapter, but no production CSPRNG adapter.

Environment note: this repo's reference Linux toolchain ships the libsodium and
libargon2 **runtime** objects but not their development headers, and the local
build does not use the vcpkg toolchain. The argon2id-backed hasher is therefore
**compiled only where the headers are present** (the vcpkg/Docker build); the
portable foundation below builds everywhere.

## Decision

1. **Library choice.**
   - **argon2id** for password-at-rest, via **libsodium**
     (`crypto_pwhash_str` / `crypto_pwhash_str_verify` / `crypto_pwhash_str_needs_rehash`,
     the `ARGON2ID13` algorithm). libsodium is added to `vcpkg.json`. Rationale:
     memory-hard, side-channel-resistant, self-describing PHC string output
     (`$argon2id$v=19$m=...,t=...,p=...$salt$digest`) that encodes its own
     parameters — so `needs_rehash` is a library call, not bespoke parsing.
   - **SRP-6a (wire)**: keep the audited in-tree implementation under
     `src/infra/crypto/` (`bnet_srp3`, `nls`). Replacing the algorithm would
     risk silent client lockout; the win is constant-time comparison and a
     single implementation, both achievable in-tree. SRP bit-compatibility is
     pinned by golden-vector tests captured from real client handshakes.
   - **WoL hashing**: keep the algorithm (wire-fixed); one audited impl in
     `src/infra/crypto/wol_hash.*`.
   - **Randomness**: one canonical CSPRNG, `core::crypto::SecureRandom`. It
     reads the OS entropy source (`std::random_device`, backed by
     `/dev/urandom` / `getrandom` on Linux); when built with libsodium
     (`PVPGN_V3_WITH_SODIUM`) it delegates to `randombytes_buf`. `std::rand()`
     is banned in `src/`.

2. **`core/crypto/` module** (portable, no third-party deps):
   - `core::crypto::IPasswordHasher` — the **at-rest** hashing port
     (`hash` / `verify` / `needs_rehash` / `algorithm`). Distinct from the
     legacy *session-hash* port `domain::identity::IPasswordHasher`, which
     derives the per-connection bnet session hash and is unrelated to
     password storage.
   - `core::crypto::SecureRandom` — the canonical CSPRNG wrapper.
   - The concrete **`Argon2idPasswordHasher`** adapter lives in
     `src/infra/crypto/` and is built only when libsodium is available.

3. **At-rest migration (transparent upgrade).** Accounts gain a
   `hash_version` discriminator. On a successful login verified against a
   legacy `bnethash` digest, the plaintext is transparently re-hashed with
   argon2id and persisted; both columns are kept for one major-release
   deprecation window, after which the legacy column is dropped. New accounts
   store argon2id only.

4. **Argon2id parameters.** Default `t=2, m=64 MiB, p=1`
   (`crypto_pwhash_OPSLIMIT_MODERATE`-class), exposed under `[auth.argon2id]`
   in `bnetd.toml` so operators can tune for their hardware. Conservative by
   default to avoid bricking low-end deployments (see Risks).

5. **Audit hooks.** Emit `auth.hash.algo` and `auth.hash.rehashed` metrics
   (wired in Plan 11 / observability).

## Consequences

- **Positive**: leaked digests are no longer password-equivalents; one RNG and
  one SRP implementation; self-describing hash strings make rehash decisions
  trivial; the portable `core/crypto` foundation compiles and is unit-tested
  everywhere, even without libsodium headers.
- **Negative**: the argon2id adapter is environment-gated (needs libsodium dev
  headers), so CI must run the password-hasher integration tests only on the
  libsodium-enabled build matrix. A migration window means two hash columns
  coexist transiently.

## Alternatives considered

- **bcrypt / PBKDF2**: weaker memory-hardness than argon2id; rejected for new
  storage.
- **Rewriting SRP-6a on a vendored bignum (tomcrypt)**: larger surface, same
  wire risk; the in-tree audited path with golden vectors is lower-risk.
- **`std::mt19937` for randomness**: not cryptographically secure; rejected.
  `SecureRandom` never uses a PRNG for security-sensitive bytes.

## Implementation status (2026-06-02)

- [x] This ADR.
- [x] `core/crypto/secure_random.{hpp,cpp}` + unit test (portable).
- [x] `core/crypto/password_hasher.hpp` (at-rest interface).
- [ ] libsodium added to `vcpkg.json`; `Argon2idPasswordHasher` adapter
      (env-gated).
- [ ] `account.hash_version` + transparent rehash-on-login.
- [ ] SRP golden-vector fixtures from ≥ 2 client builds per game.
- [ ] Delete `src/common/{bnethash,bnethashconv,bnetsrp3,wolhash,bigint}.*`
      (completes Plan 02's `src/common/` purge).
