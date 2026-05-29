# R281 Checklist — NLS/SRP-6a Crypto Infrastructure

## Status: Complete

## Files Created

### Library headers
- `src/v3/infra/crypto/include/infra/crypto/nls.hpp`
  - `NlsContext` struct: `salt` (32 B), `server_private_key` (32 B),
    `server_public_key` (128 B), `session_key` (40 B)
  - `NlsServer` class: `create_challenge()`, `verify_proof()`
  - `NlsError` enum: `InvalidProof`, `InvalidPublicKey`, `CryptoError`

- `src/v3/infra/crypto/include/infra/crypto/nls_verifier.hpp`
  - `NlsVerifier` class: `create_verifier()`, `hash_password()`

### Library implementation
- `src/v3/infra/crypto/src/nls.cpp`
  - Full NLS/SRP-6a server implementation using OpenSSL `BN_*` and `SHA1`
  - `NlsServer::create_challenge()` — generates random `b`, computes
    `B = (3*v + g^b mod N) mod N`
  - `NlsServer::verify_proof()` — recovers `v` from `B` and `b`, computes
    `S`, `K`, verifies `M1`, returns `M2`
  - `NlsVerifier::create_verifier()` — generates random salt, computes
    `x = H(s || H(U:P))`, `v = g^x mod N`
  - `NlsVerifier::hash_password()` — computes `x = H(s || H(U:P))`

### Build
- `src/v3/infra/crypto/CMakeLists.txt`
  - Target `infra_crypto_nls` with `PUBLIC_DEPS core OpenSSL::Crypto`

### Tests
- `tests/unit/infra/crypto/nls_test.cpp`
  - `NlsVerifier: create_verifier produces non-zero salt and verifier`
  - `NlsVerifier: same credentials produce same verifier with same salt`
  - `NlsVerifier: hash_password is case-insensitive for username/password`
  - `NlsServer: full round-trip with correct password succeeds`
  - `NlsServer: wrong password fails verification`
  - `NlsServer: tampered M1 fails verification`
  - `NlsServer: zero client public key A is rejected`

### Wiring
- `tests/unit/infra/crypto/CMakeLists.txt` — added `test_infra_crypto_nls`
  target (guarded by `if(TARGET OpenSSL::Crypto)`)
- `src/v3/CMakeLists.txt` — added `add_subdirectory(infra/crypto)` guarded
  by `if(OpenSSL_FOUND OR TARGET OpenSSL::Crypto)`

## Algorithm Parameters

| Parameter | Value |
|-----------|-------|
| N         | 1024-bit (128-byte) WAR3 NLS prime (big-endian) |
| g         | 47 (0x2F) |
| H         | SHA-1 (OpenSSL `SHA1` / `SHA_CTX`) |
| K         | 40-byte interleaved: `SHA1(S_even) \|\| SHA1(S_odd)` interleaved |
| B         | `(3*v + g^b mod N) mod N` |
| u         | `H(A \|\| B)[0..3]` as little-endian uint32 |
| S (server)| `(A * v^u mod N)^b mod N` |
| M1        | `H(H(N) XOR H(g), H(I), s, A, B, K)` |
| M2        | `H(A, M1, K)` |

## Design Notes

- `NlsContext` does **not** store the verifier `v`. Instead, `verify_proof()`
  recovers `v` from `B` and `b` via `v = (B - g^b) * modinv(3, N) mod N`.
  This avoids storing the verifier in session state.
- All OpenSSL `BIGNUM` / `BN_CTX` objects are wrapped in `unique_ptr` with
  custom deleters (RAII, no raw `delete`).
- Namespace: `pvpgn::infra::crypto` (not `pvpgn::v3::infra::crypto` — the
  task spec uses the shorter form).
- The `infra_crypto_nls` target is separate from the existing `infra_crypto`
  target to avoid adding an OpenSSL hard-dependency to the existing library.

## Checklist

- [x] Read legacy `src/bnetd/nls.cpp` / `nls.h` (files absent; used
      `src/common/bnetsrp3.cpp` and task spec as reference)
- [x] Read `src/bnetd/bn_srp.cpp/h` (absent; `src/v3/infra/crypto/bnet_srp3`
      used as reference for v3 patterns)
- [x] Create `src/v3/infra/crypto/include/infra/crypto/nls.hpp`
- [x] Create `src/v3/infra/crypto/include/infra/crypto/nls_verifier.hpp`
- [x] Create `src/v3/infra/crypto/src/nls.cpp`
- [x] Create `src/v3/infra/crypto/CMakeLists.txt`
- [x] Create `tests/unit/infra/crypto/nls_test.cpp`
- [x] Update `tests/unit/infra/crypto/CMakeLists.txt`
- [x] Wire `add_subdirectory(infra/crypto)` into `src/v3/CMakeLists.txt`
- [x] Create `plans/r281-checklist.md`
- [x] Build `infra_crypto_nls` target — `libinfra_crypto_nls.a` built successfully
- [x] Build `test_infra_crypto_nls` target — binary built successfully
- [x] Run `test_infra_crypto_nls` — **248 assertions in 7 test cases, all passed**
