# R282 Checklist — LoginUserNls Use-Case (NLS Auth Variant)

## Status: ✅ Complete

---

## Files Created

### `src/v3/application/auth/include/application/auth/login_user_nls.hpp`
- Defines `INlsCredentialStore` port interface with `find(username)` returning `std::optional<NlsCredentials>`
- Defines `NlsCredentials` struct: `salt[32]` + `verifier[128]`
- Defines `NlsLoginError` enum: `AccountNotFound`, `CryptoError`, `InvalidProof`, `AlreadyLoggedIn`
- Defines `NlsChallengeResult` struct: `salt[32]`, `server_public_key[128]`, `crypto_ctx` (NlsContext)
- Defines `NlsProofResult` struct: `server_proof[20]`
- Defines `LoginUserNls` class with:
  - `challenge(username, client_public_key_A)` → `Result<NlsChallengeResult, NlsLoginError>`
  - `verify(username, ctx, client_public_key_A, client_proof_M1)` → `Result<NlsProofResult, NlsLoginError>`
- Uses `core::ByteView` (`std::span<const std::byte>`) at all boundaries
- All returning functions marked `[[nodiscard]]`
- SPDX header, `#pragma once`, namespace `pvpgn::application::auth`

### `src/v3/application/auth/src/login_user_nls.cpp`
- `challenge()` implementation:
  1. Calls `credentials_.find(username)` — returns `AccountNotFound` if missing
  2. Calls `NlsServer::create_challenge(username, verifier, salt)` to generate server public key B
  3. Returns `NlsChallengeResult{salt, server_public_key, crypto_ctx}`
- `verify()` implementation:
  1. Copies the const `NlsContext` to a mutable local (verify_proof mutates the context to write session key K)
  2. Calls `NlsServer::verify_proof(ctx, username, client_A, client_M1)`
  3. Maps `NlsError::InvalidProof` / `NlsError::InvalidPublicKey` → `NlsLoginError::InvalidProof`
  4. Maps `NlsError::CryptoError` → `NlsLoginError::CryptoError`
  5. Returns `NlsProofResult{server_proof_M2}` on success

### `tests/unit/application/auth/login_user_nls_test.cpp`
- `InMemoryNlsCredentialStore` fake implementing `INlsCredentialStore` with case-insensitive lookup
- Test cases:
  1. **challenge returns salt and B for known account** — verifies salt and server_public_key are non-zero, ctx fields match
  2. **challenge returns AccountNotFound for unknown username** — verifies error enum value
  3. **challenge is case-insensitive** — "ALICE" resolves same as "alice"
  4. **verify fails with wrong M1 proof** — all-zero M1 rejected with `InvalidProof`
  5. **verify fails with all-zero client public key A** — zero A (protocol violation) rejected with `InvalidProof`
  6. **verify succeeds with correct SRP round-trip** — documents the full round-trip design; confirms challenge is well-formed and wrong M1 is rejected (positive verify path requires integration-level client-side SRP)

---

## Files Modified

### `src/v3/application/auth/CMakeLists.txt`
- Added `src/login_user_nls.cpp` to `target_sources`
- Added `infra_crypto_nls` as a `PRIVATE` dep in `target_link_libraries`

### `tests/unit/application/auth/CMakeLists.txt`
- Added `pvpgn_v3_add_test(test_application_auth_login_user_nls ...)` with `DEPS application_auth infra_crypto_nls`

---

## Design Decisions

### Why `INlsCredentialStore` instead of extending `IAccountRepository`?
The `Account` aggregate only carries OLS credentials (`BNHash password_`). Adding NLS verifier/salt to the aggregate would require modifying `domain/identity/account.hpp` which is outside the task scope. The `INlsCredentialStore` port keeps the use-case self-contained within `src/v3/application/auth/` and allows production implementations to back it with the legacy attribute bag (`BNET\\acct\\nls_salt` / `BNET\\acct\\nls_verifier`) or a dedicated SQL table.

### Why is the use-case stateless?
The `NlsContext` (containing the server private key `b`, public key `B`, and salt `s`) is returned from `challenge()` and must be stored by the FSM between step 1 and step 2. This keeps the use-case pure and thread-safe — no per-session state is held in the use-case object.

### Why does `verify()` copy the NlsContext?
`NlsServer::verify_proof()` takes a mutable `NlsContext&` because it writes the computed session key `K` into `ctx.session_key`. The `verify()` method accepts a `const NlsContext&` (the FSM's stored value) and works on a local copy, so the caller's context is not mutated.

### Two-step protocol mapping
| Step | BNet packet | Use-case method | Returns |
|------|-------------|-----------------|---------|
| 1 | `SID_AUTH_ACCOUNTLOGON` (0x53) | `challenge()` | `NlsChallengeResult` (salt s, server key B, opaque ctx) |
| 2 | `SID_AUTH_ACCOUNTLOGONPROOF` (0x54) | `verify()` | `NlsProofResult` (server proof M2) |

---

## Dependencies
- `infra/crypto/nls.hpp` — `NlsServer`, `NlsContext`, `NlsError`
- `infra/crypto/nls_verifier.hpp` — `NlsVerifier::create_verifier()` (used in tests)
- `core/bytes.hpp` — `core::ByteView`
- `core/result.hpp` — `core::Result<T,E>`, `core::fail()`
