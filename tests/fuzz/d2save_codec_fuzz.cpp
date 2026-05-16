// SPDX-License-Identifier: GPL-2.0-or-later
// LibFuzzer entry point for D2 save file codec fuzzing

#include <cstdint>
#include <cstddef>

// Forward declarations for D2Save codec (would be included in real build)
// #include "protocol/d2save/codec.hpp"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 16) return 0;  // Minimal D2S header size

    // Try to parse as D2 save file
    // In a real build, this would call:
    // auto result = pvpgn::protocol::d2save::D2SaveCodec::parse(data, size);
    // The invariant is: don't crash on any input

    // Placeholder: just validate the data pointer is accessible
    (void)data;
    (void)size;

    return 0;
}
