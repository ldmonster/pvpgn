// SPDX-License-Identifier: GPL-2.0-or-later
// LibFuzzer entry point for D2 save file codec fuzzing.
// Build with: clang++ -fsanitize=fuzzer,address ...
//
// Wires the fuzzer to the real D2SaveCodec::parse() path:
//   1. parse() validates the signature, version, and minimum size.
//      Any error returned is silently discarded; the invariant is
//      "no crash on any input".
//   2. On a successful parse we also exercise extract_char_name(),
//      extract_level(), and extract_class() to cover more decode paths.

#include "protocol/d2save/codec.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    const std::span<const uint8_t> buf{data, size};

    // Step 1: attempt to parse the raw bytes as a D2 save file.
    //   - Err(InvalidArgument) → too small or bad signature/version; not a crash.
    //   - Ok(D2SaveFile)       → parsed successfully; exercise further helpers.
    auto result = pvpgn::protocol::d2save::D2SaveCodec::parse(buf);
    if (!result) return 0;  // decode error — not a crash

    // Step 2: exercise additional extraction helpers on the parsed save.
    // Any error returned by these helpers is intentionally ignored;
    // the only invariant we enforce is "no crash / no UB".
    (void)pvpgn::protocol::d2save::D2SaveCodec::extract_char_name(buf);
    (void)pvpgn::protocol::d2save::D2SaveCodec::extract_level(buf);
    (void)pvpgn::protocol::d2save::D2SaveCodec::extract_class(buf);
    (void)pvpgn::protocol::d2save::D2SaveCodec::verify_checksum(buf);

    return 0;
}

// ---------------------------------------------------------------------------
// Fallback main() for non-fuzzing builds (PVPGN_ENABLE_FUZZING=OFF).
// When LibFuzzer is not linked, LLVMFuzzerTestOneInput is never called by
// the runtime, so we provide a trivial driver that just returns success.
// ---------------------------------------------------------------------------
#ifndef PVPGN_FUZZING_ENABLED
int main() { return 0; }
#endif
