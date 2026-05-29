# R275 — Wire bnet_codec_fuzz.cpp to Actual Codec

## Status: COMPLETE

## Files Modified
- `tests/fuzz/bnet_codec_fuzz.cpp` — wired to `decode_client()` via `next_frame()`
- `tests/fuzz/CMakeLists.txt` — added `protocol_bnet` + `protocol_common` link deps

## Notes

### decode_client() signature
```cpp
// src/v3/protocol/bnet/include/protocol/bnet/codec.hpp
core::Result<ClientMessage> decode_client(const Packet& pkt);
```
Takes a `const pvpgn::protocol::Packet&` (struct with `BnetHeader header` +
`core::ByteView payload`) and returns `core::Result<ClientMessage>`.

### next_frame() return type
```cpp
// src/v3/protocol/common/include/protocol/common/next_frame.hpp
[[nodiscard]] inline core::Result<std::optional<FrameView>, DecodeError>
next_frame(core::ByteView buf) noexcept;
```
Returns:
- `Err(DecodeError)` — malformed stream (bad marker, size < 4); not a crash.
- `Ok(nullopt)` — buffer too short for a complete frame; not a crash.
- `Ok(FrameView{header, payload, opcode})` — one complete frame ready to decode.

### Bridging FrameView → Packet
`next_frame()` returns a `FrameView` (header ByteView + payload ByteView +
opcode uint8_t), while `decode_client()` expects a `Packet` (BnetHeader struct
+ payload ByteView).  The harness reconstructs the `BnetHeader` fields from the
`FrameView` fields (marker = `kBnetMarker`, code = `fv.opcode`, size = 4 +
`fv.payload.size()`) and passes the payload view directly — no copies.

### Fallback main() for non-fuzz builds
`bnet_codec_fuzz.cpp` defines a trivial `int main() { return 0; }` guarded by
`#ifndef PVPGN_FUZZING_ENABLED`.  The CMakeLists adds
`-DPVPGN_FUZZING_ENABLED` only when building the real fuzzer target, so the
fallback fires in the stub target (`fuzz_bnet_codec_stub`, `EXCLUDE_FROM_ALL`)
used for syntax-checking in non-fuzzing builds.

### CMake target names
- `protocol_bnet` — STATIC library defined in `src/v3/CMakeLists.txt` line 370;
  exposes `src/v3/protocol/bnet/include` as a public include directory.
- `protocol_common` — INTERFACE library defined in `src/v3/CMakeLists.txt`
  line 352; exposes `src/v3/protocol/common/include` as a public include
  directory.  Pulled in transitively by `protocol_bnet` but listed explicitly
  for clarity.
