# R279 Checklist — Fix golden_replay_test.cpp: Wire actual decode_client() calls

## Status: ✅ Complete

---

## What Was Found in the Existing Test File

`tests/unit/protocol/bnet/golden_replay_test.cpp` contained:

- Four `TEST_CASE` blocks with hardcoded byte arrays for `SID_NULL` (0x00, 4 bytes)
  and `SID_PING` (0x25, 8 bytes with cookies `0xDEADBEEF` and `0x12345678`).
- All actual decode calls were **commented out** with `// In a real build:` guards.
- Only placeholder `CHECK` assertions on raw byte values were present.
- The file used `std::vector<uint8_t>` and a custom `expect_bytes_equal()` helper
  that was never called.
- No includes for `protocol/bnet/codec.hpp` or `protocol/common/packet.hpp`.

---

## What Decode Calls Were Wired

| Test Case | Direction | Byte Array | Decode Function |
|-----------|-----------|------------|-----------------|
| `BNetGoldenReplay/SID_NULL_Packet` | client | `{0xFF,0x00,0x04,0x00}` | `decode_client()` |
| `BNetGoldenReplay/SID_NULL_Server` | server | `{0xFF,0x00,0x04,0x00}` | `decode_server()` |
| `BNetGoldenReplay/SID_PING_Packet` | client | `{0xFF,0x25,0x08,0x00,0xEF,0xBE,0xAD,0xDE}` | `decode_client()` |
| `BNetGoldenReplay/SID_PING_Server` | server | `{0xFF,0x25,0x08,0x00,0xEF,0xBE,0xAD,0xDE}` | `decode_server()` |
| `BNetGoldenReplay/RoundTrip_SID_NULL` | client | `{0xFF,0x00,0x04,0x00}` | `decode_client()` |
| `BNetGoldenReplay/RoundTrip_SID_PING` | client | `{0xFF,0x25,0x08,0x00,0x78,0x56,0x34,0x12}` | `decode_client()` |

The decode path for each test:
1. `std::as_bytes()` converts `std::array<uint8_t,N>` → `core::ByteView`
2. `protocol::parse_packet(ByteView)` → `FramedPacket` (validates marker, size)
3. `decode_client()` / `decode_server()` on `fp.value().packet` → `core::Result<ClientMessage>` / `core::Result<ServerMessage>`

---

## What Assertions Were Added

### SID_NULL tests
- `REQUIRE(fp.has_value())` — frame parsed without error
- `REQUIRE(result.has_value())` — decode succeeded
- `REQUIRE(std::holds_alternative<Null>(result.value()))` — correct variant

### SID_PING tests
- `REQUIRE(fp.has_value())` — frame parsed without error
- `CHECK(fp.value().packet.header.code == kSidPing)` — opcode matches
- `REQUIRE(result.has_value())` — decode succeeded
- `REQUIRE(std::holds_alternative<Ping>(result.value()))` — correct variant
- `CHECK(std::get<Ping>(result.value()).ticks == 0xDEADBEEFu)` — cookie field correct

### Round-trip tests
- `CHECK(fp.value().packet.header.marker == protocol::kBnetMarker)` — marker byte
- `CHECK(fp.value().packet.header.code == kSidNull/kSidPing)` — opcode
- `CHECK(fp.value().packet.header.size == 4/8)` — declared size
- `CHECK(fp.value().packet.payload.empty())` — empty payload for NULL
- `CHECK(fp.value().packet.payload.size() == 4)` — 4-byte payload for PING
- `CHECK(std::get<Ping>(result.value()).ticks == 0x12345678u)` — cookie field

---

## CMakeLists.txt

**No changes needed.**

`tests/unit/protocol/bnet/CMakeLists.txt` already links `protocol_bnet` for the
`test_protocol_bnet_golden_replay` target. The `protocol_bnet` library transitively
provides `protocol_common` (which contains `packet.hpp`), so no additional
`DEPS` entry is required.

---

## Files Modified

| File | Change |
|------|--------|
| `tests/unit/protocol/bnet/golden_replay_test.cpp` | Full rewrite: removed placeholder stubs, added real `parse_packet()` + `decode_client()`/`decode_server()` calls with `REQUIRE`/`CHECK` assertions |
| `plans/r279-checklist.md` | Created (this file) |

---

## Coding Standards Applied

- `// SPDX-License-Identifier: GPL-2.0-or-later` header present
- Namespace `pvpgn::protocol::bnet::test`
- `std::array<uint8_t, N>` + `std::as_bytes()` instead of `std::vector`
- `core::ByteView` = `std::span<const std::byte>` used via helper template
- Catch2 v3 macros: `TEST_CASE`, `REQUIRE`, `CHECK`
- No raw `new`/`delete`; all RAII
- C++20 `std::span` and `std::as_bytes` used throughout
