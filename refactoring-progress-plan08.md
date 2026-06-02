# Plan 08 — Crypto Modernization — Progress

> Started: 2026-06-02
> Plan: `plans/08-crypto-modernization.md` · ADR: `docs/adr/0008-crypto-libraries.md`
> Status legend: ✅ Done | 🔄 In Progress | ⬜ Not Started | 🔒 Env-gated

## Environment constraints

- The reference Linux toolchain has libsodium/libargon2 **runtime** objects but
  **no dev headers** (`sodium.h` / `argon2.h` absent), and the local build does
  not use the vcpkg toolchain. → the concrete **argon2id adapter cannot be
  compiled/verified here**; it is gated behind `find_package`/`PVPGN_V3_WITH_SODIUM`
  (same pattern as the SQLite gating).
- The portable `core/crypto` foundation (interfaces + `SecureRandom` over
  `std::random_device`) builds and is unit-tested everywhere.

## Done (2026-06-02) — portable foundation

- [x] **ADR 0008** (`docs/adr/0008-crypto-libraries.md`, Accepted): libsodium
      argon2id for at-rest; keep audited in-tree SRP-6a (`infra/crypto`); one
      canonical CSPRNG; argon2id params `t=2,m=64MiB,p=1` under `[auth.argon2id]`.
- [x] **`core::crypto::SecureRandom`** (`core/crypto/secure_random.{hpp,cpp}`):
      OS-entropy CSPRNG. Default `std::random_device`; delegates to
      `randombytes_buf` when built with libsodium. `fill_bytes` / `next_u32` /
      `next_u64` / bias-free `uniform(min,max)` (rejection sampling) + free
      `secure_random_bytes()` (thread-local). **No PRNG used for secure bytes.**
- [x] **`core::crypto::IPasswordHasher`** (`core/crypto/password_hasher.hpp`):
      the at-rest port — `hash` / `verify` (constant-time) / `needs_rehash` /
      `algorithm`. Doc-distinguished from the legacy *session-hash* port
      `domain::identity::IPasswordHasher`.
- [x] **CMake**: `secure_random.cpp` added to the `core` target;
      libsodium auto-detected via `pkg_check_modules` → sets
      `PVPGN_V3_WITH_SODIUM` + links it when present (not present locally).
- [x] **Tests**: `tests/unit/core/secure_random_test.cpp` (7 cases /
      10011 assertions — fill/empty/uniform-bounds/full-range/non-degenerate);
      both new headers added to the `core` header self-containment check (R213).
- [x] **Build verified**: `core`, `test_core_secure_random`,
      `test_core_headers_selfcontained` all green; configure logs
      "libsodium not found — SecureRandom uses std::random_device".

## `std::rand()` audit (acceptance criterion 5)

- The **only** `rand()` call in `src/` is `src/common/bigint.cpp:568` — a legacy
  Plan-08 crypto file, **not compiled** by the v3 build. It is removed when the
  `src/common/` crypto files are deleted (step below), which satisfies the
  "no `std::rand()` in `src/`" criterion outright.

## Done (2026-06-02) — argon2id adapter (env-gated)

- [x] **`Argon2idPasswordHasher`** (`infra/crypto/argon2id_password_hasher.{hpp,cpp}`)
      implements `core::crypto::IPasswordHasher` over libsodium
      `crypto_pwhash_str` / `_verify` / `_needs_rehash` (ARGON2ID13, PHC
      string output). `needs_rehash` treats both "params changed" (1) and
      "unrecognised digest / legacy" (-1) as needing a rehash. Cost params via
      `Argon2idParams` (default `t=2, m=64 MiB`, ADR 0008), ready to load from
      `[auth.argon2id]`.
- [x] **CMake gating**: source added to `infra_crypto` only `if(SODIUM_FOUND)`
      (probed via `pkg_check_modules` by the `core` block); `PVPGN_V3_WITH_SODIUM`
      + include/link propagated. Without libsodium, `infra_crypto` builds
      exactly as before (verified locally).
- [x] **`libsodium`** added to `vcpkg.json` dependencies.
- [x] **Verification despite no local libsodium headers**:
      - `infra_crypto` builds green with the adapter gated out.
      - The adapter `.cpp` passes `-fsyntax-only -Wall -Wextra` against a
        faithful stub of the documented libsodium signatures (type-correct).
      - The adapter header compiles standalone with no libsodium present.

## Done (2026-06-02) — transparent upgrade policy (plaintext-bearing auth)

**Architectural finding:** classic bnet OLS/NLS auth is challenge-response — the
server never receives the plaintext (only `hash2`/SRP proof) and must keep
`hash1`/the SRP verifier to validate. So argon2id-of-plaintext at rest applies
**only to plaintext-bearing flows** (telnet plaintext login, account-creation /
password-set carrying plaintext, a future web/admin API). A blanket
`hash_version`-on-`Account` + "rehash on every login" would be wrong for the
challenge-response paths. (User decision: build the pure, testable policy now;
defer the `Account.hash_version` field until the storing flows are settled.)

- [x] **`application::auth::PasswordUpgrade`**
      (`application/auth/password_upgrade.{hpp,cpp}`): pure, I/O-free policy.
      `verify(stored_encoded, plaintext) -> {verified, upgraded_hash?}` —
      verifies via `core::crypto::IPasswordHasher`, and on a match that
      `needs_rehash`, re-hashes the plaintext for the caller to persist. Never
      upgrades on a mismatch (fails closed).
- [x] **`StubPasswordHasher`** (deterministic, test-only) modelling a
      two-version scheme so the upgrade path is observable without libsodium.
- [x] **Tests** (`tests/unit/application/auth/password_upgrade_test.cpp`,
      5 cases / 16 assertions): current-version → no upgrade; legacy → verify +
      upgrade; wrong password → fails closed (both versions); unrecognised hash
      → fails closed; fresh round-trip. **Green.**
- [x] Wired into the `application_auth` library (the `src/CMakeLists.txt`
      target tests link; the parallel `pvpgn_application_auth` subdir definition
      updated too for consistency).

## Done (2026-06-02) — SRP-3 golden vectors

- [x] **`tests/unit/infra/crypto/bnet_srp3_golden_test.cpp`** pins the
      Battle.net SRP-3 (`infra::crypto::BnetSrp3`) bit-compatibility two ways:
      - **Round-trip (self-validating):** a full deterministic client/server
        handshake (fixed username/password/salt/`a`/`b` via the determinism
        hooks) asserts the SRP invariant — both sides derive the **same session
        key K** — and that the client and server password proofs agree (mutual
        auth). Needs no captured fixtures.
      - **Frozen wire vectors (characterization):** exact hex of `v`, `A`, `B`,
        `K`, `M1`, `M2` frozen from the parity-verified implementation; any
        change to the on-wire SRP bytes fails the test.
      - 2 cases / 8 assertions, **green**. Builds without OpenSSL/libsodium
        (`BnetSrp3` lives in `infra_crypto`).
- [ ] Augment with vectors captured from ≥ 2 real client builds per game
      (the acceptance criterion's external-fixture half; needs live captures —
      the harness above is ready to receive them).

## Remaining
- [ ] **`Account.hash_version` + storage**: add the discriminator + argon2id
      string field to `identity::Account` and the plaintext-bearing flows that
      call `PasswordUpgrade`; dual legacy+argon2id window. (Deferred — needs the
      storing flows pinned down.)
- [~] **SRP golden vectors**: harness + frozen vectors + round-trip DONE
      (`bnet_srp3_golden_test`); still TODO: capture real handshakes from ≥ 2
      client builds per game and add them as additional vectors; extend to
      `nls` (NLS/SRP-6a, OpenSSL-gated).
- [ ] **Delete** `src/common/{bnethash,bnethashconv,bnetsrp3,wolhash,bigint}.*`
      (also completes Plan 02's `src/common/` purge + removes the last
      `std::rand()` and the `setup_*.h` crypto consumers).
- [ ] `auth.hash.algo` / `auth.hash.rehashed` metrics (Plan 11).

## Acceptance criteria status

- [ ] New accounts store argon2id only — pending adapter + wiring
- [ ] Existing accounts upgrade on login — pending migration
- [~] SRP golden-vector tests pass — harness + frozen vectors green (bnet_srp3_golden_test); real-client captures still to add
- [~] No file in `src/common/` implements crypto — pending crypto deletion
- [~] No `std::rand()` in `src/` — only the to-be-deleted `bigint.cpp` remains

## Acceptance criteria status (updated)

- [~] New accounts store argon2id only — **hasher ready**; needs the
      account-creation flow wired to it (env-gated for live verification)
- [ ] Existing accounts upgrade on login — pending at-rest migration
- [~] SRP golden-vector tests pass — harness + frozen vectors green (bnet_srp3_golden_test); real-client captures still to add
- [~] No file in `src/common/` implements crypto — pending crypto deletion
- [~] No `std::rand()` in `src/` — only the to-be-deleted `bigint.cpp` remains

## Log
- 2026-06-02: ADR 0008 + `core/crypto` foundation (SecureRandom + at-rest
  IPasswordHasher interface) landed and tested; argon2id adapter env-gated.
- 2026-06-02: `Argon2idPasswordHasher` (libsodium) written + CMake-gated +
  `vcpkg.json` updated; verified by syntax-check against stub libsodium API
  (real build env-gated).
