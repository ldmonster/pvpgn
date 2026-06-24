# Crypto / Hashing parity audit — ORIGINAL vs v3

Scope: broken-SHA-1 / bnet hash, hash→hex conversion, NLS/SRP (WAR3/W3XP login),
WOL hash.

Reference (read-only): `/home/cnupt/work/pvpgn-server`
v3 (read-only): `/home/cnupt/work/pvpgn`

---

## SUMMARY

- BUG findings: **1 CRIT**, **1 LOW** (= 2 total)
- UNSURE findings: **1**
- Files that are faithful ports (explicit clean coverage): bnethash, bnethashconv, wolhash, bnetsrp3 (the SRP-3 class itself).

**Highest-severity finding: CRIT — F1.** The WAR3/W3XP login opcodes
(SID_AUTH_ACCOUNTLOGON 0x53 / SID_AUTH_ACCOUNTLOGONPROOF 0x54) are wired in v3
to a *brand-new* OpenSSL SRP-**6a** implementation (`nls.cpp`) that uses a
different prime (1024-bit vs the original 256-bit), a different public-key
formula (`B = 3v + g^b` vs `B = v + g^b`), and standard big-endian SHA-1 instead
of the original's byte-swapped `little_endian_sha1_hash`. No real WAR3 client can
authenticate against it. The faithful SRP-3 port (`bnet_srp3.cpp`) exists but is
not connected to the login path.

---

## F1 — WAR3/W3XP login uses incompatible SRP-6a instead of legacy SRP-3 (CRIT, BUG)

**Severity:** CRIT
**Classification:** BUG (v3 clearly *intends* to serve the same Battle.net WAR3
login opcodes — comments throughout reference `bnetsrp3.cpp` and the same javaop
SRP document — but the algorithm it ships is wire-incompatible).

### Original behavior

WAR3/W3XP login (`SID_AUTH_ACCOUNTLOGON`/`..PROOF`) is handled by `BnetSRP3`
(SRP-**3**):

`/home/cnupt/work/pvpgn-server/src/bnetd/handle_bnet.cpp:1928-1939`
```cpp
BigInt salt = BigInt((unsigned char*)account_salt, 32, 4, false);
BigInt verifier = BigInt((unsigned char*)account_verifier, 32, 1, false);
BnetSRP3 srp3 = BnetSRP3(username, salt);
BigInt client_public_key = BigInt((unsigned char*)conn_client_public_key, 32, 1, false);
BigInt server_public_key = srp3.getServerSessionPublicKey(verifier);
server_public_key.getData(...&server_public_key, 32, 4, false);   // 32-byte B
BigInt hashed_server_secret_ = srp3.getHashedServerSecret(client_public_key, verifier);
```

Key original parameters (`/home/cnupt/work/pvpgn-server/src/common/bnetsrp3.cpp`):
- `bnetsrp3_N` = **32 bytes** (256-bit), `0xF8FF1A8B61991803...` (line 47-52)
- `g = 0x2F` (line 45)
- Server public key: `B = (v + g.powm(b,N)) % N`  — **no `3*v` term** (line 269)
- ALL hashing is `little_endian_sha1_hash` = real SHA-1 with each output word
  byte-swapped (lines 161,166,224,225,294,303,318)
- A, B, verifier, salt are all **32-byte** values; session key K is 40 bytes.

### v3 behavior

The same opcodes are routed to the new NLS use-case, NOT to the SRP-3 port:

`/home/cnupt/work/pvpgn/src/application/connection/src/connection_fsm_authenticating.cpp:6-7,58-60`
```cpp
///   on_auth_accountlogon()      — SID_AUTH_ACCOUNTLOGON (0x53): NLS SRP step 1
///   on_auth_accountlogonproof() — SID_AUTH_ACCOUNTLOGONPROOF (0x54): NLS SRP step 2
if (login_user_nls_ != nullptr) {
    auto result = login_user_nls_->challenge(uname, key_view);
```

`LoginUserNls` → `NlsCryptoAdapter` → `NlsServer` in
`/home/cnupt/work/pvpgn/src/infra/crypto/src/nls.cpp`, which uses:
- `kNlsPrimeN` = **128 bytes** (1024-bit), `0xF488FD58...` (line 55-72) — a totally
  different modulus.
- `B = (3*v + g^b mod N) mod N` (line 369, 383) — SRP-**6a** multiplier `k=3`,
  absent from the original.
- Standard **big-endian** OpenSSL `EVP_sha1` everywhere (lines 162, 242, 271-272,
  292, 305), never the byte-swapped `little_endian_sha1_hash`.
- A/B/S/verifier are **128 bytes** (`nls.cpp:344, 418, 493-494`;
  `nls_crypto.hpp:44` `std::array<std::byte,128> server_public_key`).

The original W3 reply packet field `server_loginreply_w3.server_public_key` is
**32 bytes** (`handle_bnet.cpp:1935`), and the v3 FSM itself even documents the A
value as 32 bytes (`connection_fsm_authenticating.cpp:36` "client_key (32 bytes…)")
while `nls.cpp::verify_proof` rejects anything that is not exactly 128 bytes
(`nls.cpp:418`). Internally contradictory and externally wrong.

### Precise divergence

Three independent, each-fatal differences from the original on the *same* login
opcodes:
1. Modulus N: 256-bit `0xF8FF1A8B…` → 1024-bit `0xF488FD58…`.
2. Server public key: `B = v + g^b` → `B = 3v + g^b`.
3. Hash convention: byte-swapped (`little_endian_sha1_hash`) → canonical
   big-endian SHA-1.

Any of these makes the client's M1 proof mismatch the server's, so every WAR3
login fails (`InvalidProof`). The verifier/salt sizes (128 vs 32) also break the
stored-credential format and the wire packets.

### Proposed fix

Wire the WAR3/W3XP login opcodes to the faithful SRP-3 port that already exists,
`/home/cnupt/work/pvpgn/src/infra/crypto/src/bnet_srp3.cpp` (class `BnetSrp3`),
mirroring `handle_bnet.cpp:1928-1945`:
- challenge: `B = getServerSessionPublicKey(verifier)`, send 32-byte B + 32-byte salt;
- proof: `getHashedServerSecret` → `getClientPasswordProof` (compare) →
  `getServerPasswordProof` (reply M2).
Keep the 256-bit N (`kModulusBytes`) and `little_endian_sha1_hash` semantics.

If `nls.cpp` (SRP-6a, 1024-bit) is meant for a *different* product (e.g. a
Diablo II / WoW-style NLS), it must be gated behind that product's opcodes, not
the WAR3 SID_AUTH_ACCOUNTLOGON path — and the WAR3 path must use `BnetSrp3`.

---

## F2 — `nls.cpp` recovers `v` from `B` via modular inverse of 3 (LOW, BUG-adjacent)

**Severity:** LOW (only relevant if F1's SRP-6a path is ever kept)
**Classification:** UNSURE / design smell.

`/home/cnupt/work/pvpgn/src/infra/crypto/src/nls.cpp:460-490` reconstructs the
verifier inside `verify_proof` by `v = (B - g^b) * modinv(3,N) mod N`, because the
context never stores `v`. This is mathematically valid for the chosen `B = 3v+g^b`
form, but it is an artifact of `nls.cpp` having invented its own SRP-6a; the
original simply keeps `v` (the stored verifier) around. Not a parity bug against
the original per se — there is no original counterpart — but flagged because it
only works for the (wrong, per F1) `3*v` formula and silently couples the two.
If F1 is fixed by switching to SRP-3 this code is deleted entirely.

---

## Faithful-port confirmations (clean coverage)

### bnethash.cpp  →  bnet_hash.cpp — MATCH
`/home/cnupt/work/pvpgn-server/src/common/bnethash.cpp` vs
`/home/cnupt/work/pvpgn/src/infra/crypto/src/bnet_hash.cpp`.
- IV constants identical: `0x67452301 0xefcdab89 0x98badcfe 0x10325476 0xc3d2e1f0`
  (orig 47-51 / v3 35-39).
- The famous Blizzard "broken SHA-1" bug is reproduced correctly: message schedule
  uses `ROTL32(1, mix)` for the Blizzard variant vs `ROTL32(mix, 1)` for true SHA-1
  (orig 62-64 / v3 50-53). v3 preserves the swapped-arguments breakage.
- Round constants `0x5a827999 / 0x6ed9eba1 / -0x70e44324 / -0x359d3e2a`, rotate
  amounts (5, 30), and the f-functions match exactly (orig 72-110 / v3 65-97).
- `hash_set_16` byte packing (Blizzard little-endian byte order vs SHA-1
  big-endian with 0x80 sentinel) matches byte-for-byte (orig 126-184 / v3 108-166).
- `sha1_hash` padding branches (>=64 / >55 double-block / else) and
  `hash_set_length` bit-count packing match (orig 224-300 / v3 170-255).
- Empty-input quirk (no padding, returns the IV) is intentionally preserved and
  documented (v3 223-228).
- `little_endian_sha1_hash` byteswap reproduced via `byteswap32` (v3 257-264);
  verified the original `bn_int_nset`+`bn_int_get` pair
  (`/home/cnupt/work/pvpgn-server/src/common/bn_type.cpp:281-296, 441-444`)
  is exactly a 32-bit endian swap.
- Hex formatting `to_hex` is lowercase `%08x`, host word order — matches
  `hash_get_str` (orig 344-345 / v3 266-278). `from_hex` requires exactly 40 chars,
  matching `hash_set_str`'s `5*8` length check (orig 383 / v3 289).

### bnethashconv.cpp  →  bnet_hash_conv.cpp — MATCH
Both convert between the 5×uint32 digest and 20 wire bytes treating each word as
**little-endian** (`bn_int_get`/`bn_int_set` are LE; v3 `load_le32`/`store_le32`
are LE). Equivalent (orig 44-45,64-65 / v3 11-25,29-64).

### wolhash.cpp  →  wol_hash.cpp — MATCH
`/home/cnupt/work/pvpgn-server/src/common/wolhash.cpp` vs
`/home/cnupt/work/pvpgn/src/infra/crypto/src/wol_hash.cpp`.
- 8-byte max input enforced (orig 48 returns -1 / v3 throws — behavioral wrapper
  difference only, same boundary).
- Alphabet identical:
  `"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789./"`
  (orig `wolhash.h:29 WOL_HASH_CHAR` / v3 `wol_hash.hpp:27 kWolHashAlphabet`).
- Core loop reproduced exactly, including the sentinel read `pwd1[esi]` and the
  `esi` countdown: `if(pwd1[i]&1){ edx=pwd1[i]<<1; edx&=pwd1[esi]; } else edx=pwd1[i]^pwd1[esi];`
  (orig 65-76 / v3 28-38) and the `&0x3f` 8-char output (orig 80-83 / v3 42-45).

### bnetsrp3.cpp  →  bnet_srp3.cpp (the SRP-3 CLASS) — MATCH
`/home/cnupt/work/pvpgn-server/src/common/bnetsrp3.cpp` vs
`/home/cnupt/work/pvpgn/src/infra/crypto/src/bnet_srp3.cpp`.
- Constants match: `g=0x2F`, 32-byte N `0xF8FF1A8B…`, 20-byte I `0xF8018CF0…`
  (orig 45-58 / v3 21-32).
- Username/password upper-cased before hashing via `safe_toupper` semantics
  (orig 80-98 / v3 ascii_upper 60-73, 133/143). Locale caveat only (toupper on
  non-lowercase ASCII is a no-op; >0x7f bytes pass through in both).
- `getClientPrivateKey` = LE-SHA1( s(32) || LE-SHA1("USER:PASS") ) — matches
  (orig 148-169 / v3 183-206).
- `getScrambler` = first uint32 (native order) of *true* SHA1(raw_B) — matches;
  v3 reads `h[0]` which equals the original `*(uint32_t*)hash` (orig 171-183 /
  v3 208-220).
- `getClientSecret`/`getServerSecret`/session-key formulas, `B = (v+g^b)%N`
  (no `3v`), interleaved odd/even `hashSecret`, and both password proofs match
  byte-layout for byte-layout (orig 185-321 / v3 222-350), all using the
  byte-swapped LE SHA-1.
NOTE: this faithful port is the one that *should* serve WAR3 login (see F1) but is
currently bypassed.

---

## Items checked and cleared (not bugs)

- IV / round constants / rotate amounts in the broken SHA-1: identical.
- Endianness of digest↔wire conversion: both LE, consistent.
- Hex case (lowercase) and zero-padding: identical.
- `little_endian_*` byteswap derivation from `bn_int_nset`/`bn_int_get`: verified
  equivalent to a 32-bit endian swap.
- WOL alphabet, sentinel access, output length: identical.
