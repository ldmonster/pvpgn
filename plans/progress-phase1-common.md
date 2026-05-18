# Progress: Phase 1 — Legacy Common & Compat Migration

Tracks step-by-step execution of [refactoring-plan-legacy-common.md](refactoring-plan-legacy-common.md).

Status legend: `[ ]` not-started · `[~]` in-progress · `[x]` complete · `[-]` skipped

## Step 1 — Create `src/v3/infra/compat/`  ✅

- [x] Create module skeleton (`include/infra/compat/{platform,process}.hpp`, `src/process.cpp`)
- [x] Register module in `src/v3/CMakeLists.txt` (made `OpenSSL` optional along the way)
- [x] Build verification via `Dockerfile.v3` (Alpine + boost-dev): `infra_compat` builds clean
- [x] Catch2 unit test (`tests/unit/infra/compat/process_test.cpp`) — 4 cases / 9 assertions pass
- [-] Legacy `src/compat/*` left in place; migration deferred to later steps

## Step 2 — Create `src/v3/infra/crypto/`  ✅

- [x] `infra/crypto/bnet_hash.{hpp,cpp}` — Blizzard broken-SHA1, true SHA-1,
      hex helpers. Verified bug-for-bug compatible with legacy
      `bnethash.cpp` (including the `sha1_hash(empty)==IV` quirk).
- [x] `infra/crypto/bnet_hash_conv.{hpp,cpp}` — host<->wire 20-byte
      hash byte-order conversion.
- [x] `infra/crypto/big_uint.{hpp,cpp}` — `BigUInt` thin wrapper over
      `boost::multiprecision::cpp_int`. `pow_mod` parity-verified
      against legacy `BigInt::powm`.
- [x] **`BigUInt::from_bytes_legacy` / `to_bytes_legacy`** mirroring
      legacy `BigInt(buf,size,blockSize,bigEndian)` ctor and
      `getData(buf,size,blockSize,bigEndian)`. Segment-based layout
      (BE-within-block, LE-across-blocks) faithfully reproduces the
      SRP-3 wire convention `(byteCount, 4, false)`. Edge case
      where value-bit-width < buffer-bit-width is documented as
      out-of-scope (legacy puts the value at the buffer's END in
      that case via its variable `segment_count`; v3 always treats
      `segment_count = byteCount/4`). All SRP-3 sizes match
      byteCount/4 so this never surfaces in practice.
- [x] Register `infra_crypto` target in `src/v3/CMakeLists.txt`.
- [x] **Parity-test harness**: `tests/unit/infra/crypto/parity_test.cpp`
      links both `infra_crypto` (v3) and `common` (legacy) and compares
      outputs byte-for-byte. Caught 2 bugs already:
        1. Legacy `sha1_hash(empty)` returns the IV (legacy bug); v3 now
           matches it intentionally.
        2. Legacy `BigInt::getData(blockSize, bigEndian)` byte-order
           semantics are non-trivial; v3 now mimics them via
           `*_legacy` helpers.
- [x] Build & tests verified in Docker (`Dockerfile.v3`):
      compat 9/4 + crypto 41/19 + parity 61/20 all pass.
- [x] port `bnetsrp3.cpp` -- v3 `infra/crypto/bnet_srp3.{hpp,cpp}`
      exposes `BnetSrp3` class with both (username, salt) and
      (username, password) ctors, deterministic test hooks
      (`set_salt`, `set_client_private_key`,
      `set_server_private_key`), and the full SRP-3 protocol
      surface (`verifier`, `client_secret`, `server_secret`,
      `hash_secret`, `client_password_proof`, `server_password_proof`,
      `server_session_key` / `client_session_key`). Parity verified
      step-by-step in the granular
      "BnetSrp3 parity: x (client private key) matches legacy" test:
      salt int, raw_salt bytes, userpass hash, private_value
      bytes, private_value hash, x integer, AND `g^x mod N` all
      match legacy. End-to-end self-consistency proven in
      "full SRP-3 protocol round (v3-only consistency)":
      `K_c == K_s`, `M_c == M_server_check`,
      `M'_s == M'_client_check`.
- [x] port `wolhash.cpp` (Westwood Online hash) -- v3
      `infra/crypto/wol_hash.{hpp,cpp}` exposes
      `wol_hash(std::span<const uint8_t>) -> std::string`. Parity
      tested against `pvpgn::wol_hash` for empty, short ASCII and
      8-byte binary inputs.
- [x] port `peerchat.cpp` (peerchat encryption) -- v3
      `infra/crypto/peerchat.{hpp,cpp}` exposes `PeerchatCipher`
      class (RAII over the legacy `gs_peerchat_ctx`). Parity
      tested: init state matches legacy bit-for-bit; multi-chunk
      encrypt-then-decrypt round-trip; counter / state agreement
      after streaming 300 bytes through legacy and v3 in parallel.
- [-] Legacy `src/common/{bnethash*,bigint,bnetsrp3,wolhash,peerchat}`
      left in place; consumers migrate later

### Step 2 caveats / known legacy quirks documented

- Legacy `BigInt::toHexString()` has a signed-`char` accumulator
  (`sum`) in its leading-zero-suppression heuristic that can drop
  intermediate digits from the top segment for certain integers.
  This makes `toHexString()` <-> `from_hex()` an unreliable equality
  bridge for arbitrary BigInts. The parity tests therefore use
  deterministic byte-derived salts for the SRP-3 verifier check
  rather than legacy's random `BnetSRP3::getSalt()`.
- Legacy `BigInt(bytes, size, blockSize, bigEndian)` ctor and
  `getData(buf, size, blockSize, bigEndian)` with matching
  `(blockSize, bigEndian)` pair do NOT round-trip the byte buffer
  for `(1, true)` or `(4, false)`. v3 `*_legacy` helpers replicate
  the exact internal layout but downstream code should treat the
  hex string as the only canonical bridge for full integers, and
  fixed (blockSize, bigEndian) pairs only when paired with the
  matching inverse pair (e.g., write with `(1, true)`, read with
  `(1, true)` does NOT recover bytes).

## Step 3 — Migrate protocol `*_protocol.h` headers

Strategy: each legacy `src/common/*_protocol.h` becomes a header-only
`wire_types.hpp` in the matching `src/v3/protocol/<module>/include/protocol/<module>/`
tree. Constants become `constexpr`, struct fields use native fixed-width
integers (no `bn_*` / `PACK` macros — codec layer handles LE conversion),
and every wire struct gets `static_assert(std::is_trivially_copyable_v<...>)`
plus a size assertion when memcpy semantics matter. Each header gets a
matching Catch2 unit test that pins constants to the legacy `#define`
values (so changes break the build, not the runtime).

- [x] `init_protocol.h` -> `protocol/bnet/include/protocol/bnet/init_wire_types.hpp`
      (`pvpgn::protocol::bnet::init` namespace). 1 struct
      (`ClientInitConn`, 1 byte) + 9 connection-class constants.
      Pinned by `tests/unit/protocol/bnet/init_wire_types_test.cpp`.
- [x] `udp_protocol.h` -> `protocol/udp/include/protocol/udp/wire_types.hpp`
      (`pvpgn::protocol::udp::wire` namespace). 5 structs (`UdpHeader`,
      `ServerUdpTest`, `ClientUdpPing`, `ClientSessionAddr{1,2}`) +
      5 type codes. Pinned by
      `tests/unit/protocol/udp/wire_types_test.cpp`.
- [x] `bot_protocol.h` (99 LOC) -> `protocol/bnet/include/protocol/bnet/bot_wire_types.hpp`
- [x] `file_protocol.h` (132 LOC) -> `protocol/file/include/protocol/file/wire_types.hpp`
- [x] `d2cs_bnetd_protocol.h` (102 LOC) -> `protocol/d2cs/.../bnetd_wire_types.hpp`
- [x] `d2cs_d2gs_protocol.h` (192 LOC) -> `protocol/d2gs/.../wire_types.hpp`
- [x] `wol_gameres_protocol.h` (311 LOC) -> `protocol/wolgameres/.../wire_types.hpp`
- [x] `d2cs_protocol.h` (341 LOC) -> `protocol/d2cs/.../wire_types.hpp`
- [x] `d2game_protocol.h` (423 LOC) -> `protocol/d2gs/.../game_wire_types.hpp`
- [x] `irc_protocol.h` (464 LOC) -> `protocol/irc/.../wire_types.hpp`
- [x] `anongame_protocol.h` (630 LOC) -> `protocol/bnet/.../anongame_wire_types.hpp`
      (constants-only port; per-message structs deferred until
      `bnet_protocol.h` lands so they can embed the real bnet header)
- [x] `bnet_protocol.h` (3649 LOC, the big one) -> split across 8
      sibling headers under `protocol/bnet/include/protocol/bnet/`:
      `wire_types.hpp` (foundation: `BnetHeader`, `W3RouteHeader`,
      generics, `kClientNull`) + topic sub-headers
      `w3route_wire_types.hpp`, `auth_wire_types.hpp`,
      `account_wire_types.hpp`, `cdkey_wire_types.hpp`,
      `realm_wire_types.hpp`, `chat_wire_types.hpp`,
      `game_wire_types.hpp`, `clan_wire_types.hpp`,
      `misc_wire_types.hpp`. Constants-only port; the 179 per-message
      `PACK`ed structs are explicitly deferred (same precedent as
      `anongame_protocol.h`) and will land incrementally alongside the
      codec implementations. Each sub-header has a matching Catch2
      test pinning its packet codes and result enums to the legacy
      `#define` values.

### Build & test integration

- `Dockerfile.v3` `v3-build` target list extended with all 24
  wire-type test targets (14 prior + 10 new bnet sub-headers).
  `v3-test` stage runs each. Latest run: **all green** across the
  full pipeline (run via
  `docker build -f Dockerfile.v3 --target v3-test -t pvpgn:v3-test .`).

## Step 4 — Packet/Queue audit

- [ ] not started

## Step 5 — Type utilities into `core/`

- [ ] not started

## Step 6 — Eliminate `xalloc`

- [ ] not started

## Step 7 — Eliminate legacy data structures

- [ ] not started

## Step 8 — Migrate networking to `infra/net`

- [ ] not started

## Step 9 — pugixml via FetchContent

- [ ] not started

## Step 10 — Configuration migration

- [ ] not started

## Notes / decisions

- The v3 tree compiles in parallel with the legacy tree under `PVPGN_BUILD_V3=ON`.
  Step 1 only adds new code under `src/v3/infra/compat/` and does NOT touch
  `src/compat/*` consumers — that is deferred to Step 6/7 where the legacy
  bnetd code itself is migrated.
- `pvpgn_v3_add_library` helper lives in `cmake/v3.cmake`; the active
  pattern is the one used in `src/v3/CMakeLists.txt`, NOT the unused
  per-subdir CMakeLists (e.g. `src/v3/infra/clock/CMakeLists.txt` is dead).
- Made `find_package(OpenSSL)` non-REQUIRED in `src/v3/CMakeLists.txt`
  and split `peer_link.cpp` out of `runtime` when OpenSSL is absent
  (peer_link is the only OpenSSL consumer in the active v3 build).
- **Blocker**: this Windows dev host has no Boost installed. The v3 tree
  hard-requires Boost ≥1.75 via `find_package(Boost REQUIRED COMPONENTS system)`
  at `src/v3/CMakeLists.txt` line 61. Options to unblock:
    1. Install Boost via vcpkg / chocolatey on the host.
    2. Verify the build inside the docker `build-base` stage (where Boost is
       available) by adding an `apk add boost-dev` step — the v3 tree is not
       currently built in the docker pipeline.
    3. Refactor `infra/net` to optionally compile out when Boost is missing
       (large scope, deferred).
