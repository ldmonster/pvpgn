# 06 — Protocol Layer & Codecs

**Goal:** Treat every wire protocol (BNet 1.x, IRC, Telnet, WoL, D2GS,
D2CS s2s, D2DBS s2s) as a **pure codec library** in
`src/v3/protocol/<name>/`. The codec turns bytes into typed messages
and back, with **zero** side effects, zero logging, zero global
state, and zero dependency on `infra/`.

## 1. Today's situation

- `src/v3/protocol/` exists (folder per protocol).
- Packet decoding still happens inline inside
  `integration_legacy_bnetd_linked` / `handle_bnet_link.cpp` using
  C-style `t_packet*` and `bn_byte` helpers.
- Bnet packet framing assumes little-endian + 4-byte header. The
  current code does not consistently bounds-check.

## 2. Target shape

For each protocol `P` with messages `M1, M2, …`:

```cpp
// protocol/bnet/messages.h
namespace pvpgn::protocol::bnet {

struct LoginReq1 final {
  uint32_t client_token;
  uint32_t server_token;
  std::array<uint32_t,5> password_hash;
  std::string username;
};

using ServerMessage = std::variant<
    LoginReq1, JoinChannel, ChannelMessage, ...>;

[[nodiscard]] core::Result<ServerMessage, DecodeError>
    decode(std::span<const std::byte> frame) noexcept;

[[nodiscard]] core::Bytes encode(const ServerMessage& msg);

// Framing — returns number of bytes consumed or "need more".
struct FrameView { std::span<const std::byte> payload; std::size_t total; };
[[nodiscard]] core::Result<std::optional<FrameView>, DecodeError>
    next_frame(std::span<const std::byte> buf) noexcept;

}  // namespace
```

Rules:

- Messages are POD-ish aggregates.
- `decode` is `noexcept`. Out-of-bounds, bad length, bad opcode →
  `DecodeError`.
- `encode` returns owned bytes (`core::Bytes` ≈
  `std::vector<std::byte>`).
- The codec doesn't know about sockets, connections, sessions. The
  pump (`integration/<proto>/`) feeds it bytes and dispatches the
  decoded message to a use case.

## 3. Bounds safety

Codec functions take `std::span<const std::byte>` only. No raw
pointers. No `memcpy` with hardcoded sizes — use `core::endian::read_le<T>`
helpers that take a span and an offset and return
`Result<T, DecodeError>` (offset OOB ⇒ error). After R210 (C++20),
prefer `std::bit_cast` where applicable.

A `DecodeError` is an enum class with:

- `Truncated`,
- `UnknownOpcode`,
- `MalformedString` (non-null-terminated etc.),
- `InvalidLength`,
- `UnsupportedVersion`,
- `ChecksumMismatch`.

## 4. Fuzz targets (mandatory)

Per protocol, a libFuzzer target lives in `tests/fuzz/<proto>/`:

```cpp
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t len) {
  auto bytes = std::span<const std::byte>{
      reinterpret_cast<const std::byte*>(data), len};
  while (auto frame = pvpgn::protocol::bnet::next_frame(bytes)) {
    if (!frame.value()) break;
    (void)pvpgn::protocol::bnet::decode(frame.value()->payload);
    bytes = bytes.subspan(frame.value()->total);
  }
  return 0;
}
```

CI runs each fuzz target for 60 s on every PR (smoke), and for
30 min nightly on the dev branch. Corpus is seeded from real captures
in `tests/fuzz/corpus/`.

## 5. Round-trip property tests

For every encodable message:
`decode(encode(msg)) == msg`. Use Catch2 generators (rapidcheck
optional) to exhaustively cover the shape.

## 6. Versioning & feature gates

Some Bnet messages differ between client variants (`STAR`, `D2DV`,
`D2XP`, `WAR3`, `W3XP`). Today this is handled by branchy code paths
in legacy `handle_bnet`. Future state:

- Each variant gets a sub-codec
  (`protocol/bnet/variants/{star,d2dv,d2xp,war3}.h`).
- The pump selects the variant after `CLIENT_AUTH_INFO` parses, then
  routes all subsequent frames through the variant codec.
- Common fields stay in `protocol/bnet/common.h`.

## 7. Concrete tasks

- [ ] R250: define `DecodeError` and the protocol/bnet skeleton.
- [ ] R251–R256: extract one Bnet message family per round
      (login, channel, message, game, friends, clan, ladder).
- [ ] R257: same for IRC.
- [ ] R258: same for Telnet.
- [ ] R259: same for WoL (low priority — WoL is dormant; YAGNI may
      defer or delete; see `14-legacy-retirement.md`).
- [ ] R260: same for D2 s2s (d2cs ↔ bnetd, d2dbs ↔ d2cs).
- [ ] R261: fuzz harness wired into CI.

## 8. Non-goals

- New protocols. The wire is frozen; we just type it safely.
- Encrypted transport. (BNCS doesn't use TLS; client compat forbids
  changing this.)
- Protocol-buffer / msgpack interop. Not on the roadmap.
