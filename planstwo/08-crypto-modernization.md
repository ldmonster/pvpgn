# 08 — Crypto Modernization

## What

Replace at-rest password hashing and the bespoke SRP implementation
with vetted libraries. Keep wire-format compatibility for shipped
clients.

## Scope

| Concern | Today | Wave two |
|---|---|---|
| At-rest password hash | `bnethash` (SHA-1 derivative) | argon2id via libsodium |
| Wire SRP for auth | `bnetsrp3` (hand-rolled SRP-6a) | Vetted SRP-6a impl (tomcrypt or in-tree audited rewrite) |
| WoL hashing | `wolhash` (MD5/SHA mix) | Same algorithm; one impl in `core/crypto/wol/`, audited |
| Random | `std::rand` + custom mix | `std::random_device` + libsodium `randombytes_buf` |

## Why

- `bnethash` stores password equivalents that compromise once leaked.
  Argon2id with a per-user salt is the modern baseline.
- `bnetsrp3` lacks constant-time comparisons in places; replacing it
  removes a class of timing vulnerabilities without changing the wire.
- One canonical RNG eliminates seed-handling bugs.

## Prerequisites

- libsodium vetted via vcpkg; ADR `0008-crypto-libraries.md`.
- Plan 02 partially landed (so the legacy hash files have at most one
  consumer left).

## Concrete steps

1. **ADR** for library choice.
2. **`core/crypto/`** new module exposing:
   - `password_hasher` interface (`hash`, `verify`, `needs_rehash`).
   - `srp6a_session` (server side only).
   - `secure_random` thin wrapper.
3. **At-rest migration.** On successful login under the old hash,
   transparently rehash with argon2id and store both fields during a
   deprecation window (one major release). After the window, drop the
   legacy column. Track via `account.hash_version`.
4. **Wire SRP.** Swap implementation behind `srp6a_session`; add
   golden-vector tests using captured client handshakes to prove bit
   compatibility.
5. **Delete** `src/common/{bnethash,bnethashconv,bnetsrp3,wolhash}.{cpp,h}`.
6. **Audit hooks.** Emit `auth.hash.algo` and `auth.hash.rehashed`
   metrics (plan 11).

## Acceptance criteria

- [ ] New accounts store argon2id only.
- [ ] Existing accounts transparently upgrade on next login.
- [ ] SRP golden-vector tests pass against captured fixtures from at
      least two client builds per supported game.
- [ ] No file in `src/common/` implements crypto.
- [ ] No `std::rand()` call anywhere in `src/`.

## Risks

- Argon2id parameters chosen too aggressively will brick low-end
  deployments. Default to `t=2, m=64MiB, p=1`; expose under
  `[auth.argon2id]` in TOML.
- Bit-incompat in SRP would lock out clients silently. Block the
  swap behind golden-vector tests with vectors from real client
  captures.

## Out of scope

- Changing the public account-creation flow.
- TLS termination (handled by reverse proxy in deployment).
