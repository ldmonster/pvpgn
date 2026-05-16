// SPDX-License-Identifier: GPL-2.0-or-later
// LibFuzzer entry point for BNet codec fuzzing
// Build with: clang++ -fsanitize=fuzzer,address ...

#include <cstdint>
#include <cstddef>

// Forward declarations for BNet codec (would be included in real build)
// #include "protocol/bnet/codec.hpp"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 4) return 0;

    // Try to decode as BNet packet
    // In a real build, this would call:
    // auto result = pvpgn::protocol::bnet::BnetCodec::decode(data, size);
    // The invariant is: don't crash on any input

    // Placeholder: just validate the data pointer is accessible
    (void)data;
    (void)size;

    return 0;
}
