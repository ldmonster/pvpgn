# R319 — Replace trivial checksum with CRC32 in `MigrationRunner`

## Checklist
- [x] Read `migration_runner.cpp` to identify the trivial checksum implementation
- [x] Identified `compute_checksum()` used SQL length + first/last character hex encoding
- [x] Implemented a compile-time CRC32 lookup table (`make_crc32_table()`) using IEEE 802.3 polynomial `0xEDB88320` (reflected)
- [x] Implemented `crc32(std::string_view)` function operating over UTF-8 bytes
- [x] Replaced `compute_checksum()` body to call `crc32()` and format result as 8 zero-padded hex digits
- [x] Added required `#include <array>`, `#include <cstdint>`, `#include <iomanip>` headers
- [x] No new external dependencies — CRC32 is implemented inline in an anonymous namespace
- [x] Checksum stored/compared as `uint32_t` formatted to `std::string` (8 hex chars)

## Result
Replaced the trivial `compute_checksum()` (which used only SQL length and first/last bytes) with a proper CRC32 implementation. A `constexpr` lookup table is generated at compile time using the standard IEEE 802.3 reflected polynomial `0xEDB88320`. The `crc32()` helper processes all UTF-8 bytes of the migration SQL text and returns a `uint32_t`. The checksum is stored as an 8-character zero-padded lowercase hex string (e.g., `"a1b2c3d4"`), providing strong integrity verification for migration SQL content.
