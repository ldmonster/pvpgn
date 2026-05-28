# R274 — DecodeError Enum + next_frame() Wrapper

## Status: COMPLETE

## Files Created
- src/v3/protocol/common/include/protocol/common/decode_error.hpp
- src/v3/protocol/common/include/protocol/common/frame_view.hpp
- src/v3/protocol/common/include/protocol/common/next_frame.hpp

## Files Modified
- src/v3/CMakeLists.txt

## Notes

### parse_packet() API
`parse_packet()` lives in `pvpgn::protocol` (namespace in `packet.hpp`).
Signature: `core::Result<FramedPacket> parse_packet(core::ByteView buf)`

`FramedPacket` contains:
- `packet.header`  — `BnetHeader` (marker=0xFF, code=opcode, size=total length)
- `packet.payload` — `core::ByteView` into the buffer, excluding the 4-byte header
- `consumed`       — equals `header.size`; caller advances stream cursor by this

Error codes returned by `parse_packet()`:
- `core::StatusCode::OutOfRange`     — buffer shorter than header OR shorter than
                                       declared packet size (incomplete, not fatal)
- `core::StatusCode::InvalidArgument` — marker ≠ 0xFF, or size field < 4 (fatal)

### How next_frame() wraps it
`next_frame(buf)` returns `core::Result<std::optional<FrameView>, DecodeError>`:

| parse_packet() outcome                        | next_frame() return                    |
|-----------------------------------------------|----------------------------------------|
| `OutOfRange` (incomplete buffer)              | `Ok(nullopt)` — wait for more data     |
| `InvalidArgument` + marker OK + size < 4      | `Err(DecodeError::InvalidLength)`      |
| `InvalidArgument` + bad marker                | `Err(DecodeError::Truncated)`          |
| Any other error                               | `Err(DecodeError::Truncated)`          |
| Success                                       | `Ok(FrameView{header, payload, opcode})` |

### frame_view.hpp
Uses `core::ByteView` (= `std::span<const std::byte>`) from `core/bytes.hpp`
rather than raw `std::span<const std::byte>` for consistency with the rest of
the codebase.

### CMakeLists.txt
`protocol_common` is an INTERFACE-only library defined inline in
`src/v3/CMakeLists.txt` (no separate `src/v3/protocol/common/CMakeLists.txt`
exists). The `INTERFACE_SOURCES` list was extended to include all seven headers
(the four pre-existing ones plus the three new ones) so IDEs and static
analysis tools can discover them via the CMake target.
