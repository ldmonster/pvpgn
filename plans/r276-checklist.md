# R276 — Seed Fuzz Corpus + Wire d2save_codec_fuzz.cpp

## Status: COMPLETE

## Files Created

### Corpus seed files (raw binary BNet packets)
- `tests/fuzz/corpus/bnet/0x00_null.bin` — SID_NULL (4 bytes: `FF 00 04 00`)
- `tests/fuzz/corpus/bnet/0x25_ping.bin` — SID_PING (8 bytes: `FF 25 08 00 44 33 22 11`)
- `tests/fuzz/corpus/bnet/0x50_auth_info.bin` — SID_AUTH_INFO (58 bytes: `FF 50 3A 00` + 9×u32 + "USA\0United States\0")
- `tests/fuzz/corpus/bnet/0x29_logon_request.bin` — SID_LOGONRESPONSE/LoginReq1 (38 bytes: `FF 29 26 00` + ticks + sessionkey + 5×u32 hash + "alice\0")
- `tests/fuzz/corpus/bnet/0x0a_enter_chat.bin` — SID_ENTERCHAT/EnterChatRequest (15 bytes: `FF 0A 0F 00` + "alice\0PXES\0")

### D2Save fuzz target (wired)
- `tests/fuzz/d2save_codec_fuzz.cpp` — wired to `D2SaveCodec::parse()` + extraction helpers

## Files Modified
- `tests/fuzz/CMakeLists.txt` — D2Save fuzzer target now links `protocol_d2save` and defines `PVPGN_FUZZING_ENABLED`; added `fuzz_d2save_codec_stub` for non-fuzzing syntax-check builds

## Notes

### Corpus seed sources
All seed bytes were derived from round-trip tests in `tests/unit/protocol/bnet/codec_test.cpp`:
- `0x00_null.bin`: exact bytes asserted at lines 77–88 (`FF 00 04 00`)
- `0x25_ping.bin`: exact bytes asserted at lines 90–105 (`FF 25 08 00 44 33 22 11`)
- `0x50_auth_info.bin`: field values from the `SID_AUTH_INFO round-trip` test (lines 56–75); platform `IX86`, game `SEXP`, country `USA`/`United States`
- `0x29_logon_request.bin`: field values from the `SID_LOGONRESPONSE (0x29) request round-trip` test (lines 2337–2346); ticks=0x1000, sessionkey=0xCAFE, hash pattern 0x11111111…0x55555555, name `alice`
- `0x0a_enter_chat.bin`: field values from the `SID_ENTERCHAT client+server round-trip` test (lines 205–217); username `alice`, statstring `PXES`

### D2Save codec API
`D2SaveCodec` (in `src/v3/protocol/d2save/`) exposes:
- `parse(std::span<const uint8_t>)` → `core::Result<D2SaveFile, core::Error>` — validates signature (`0xAA55AA55`), version (109/110), and minimum size (≥ `MIN_FILE_SIZE`)
- `extract_char_name(span)`, `extract_level(span)`, `extract_class(span)` — field extractors exercised after a successful parse
- `verify_checksum(span)` → `bool` — CRC32 validation

The fuzz target calls all four helpers so that any decode path reachable from raw bytes is exercised under AddressSanitizer.

### CMakeLists.txt changes
- Fuzzing build: added `target_compile_definitions(fuzz_d2save_codec PRIVATE PVPGN_FUZZING_ENABLED)` and linked `protocol_d2save` (was missing before R276)
- Non-fuzzing build: added `fuzz_d2save_codec_stub` target (mirrors the existing `fuzz_bnet_codec_stub` pattern) so `d2save_codec_fuzz.cpp` is always syntax-checked
