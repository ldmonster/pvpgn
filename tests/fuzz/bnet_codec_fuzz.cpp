// SPDX-License-Identifier: GPL-2.0-or-later
// LibFuzzer entry point for BNet codec fuzzing.
// Build with: clang++ -fsanitize=fuzzer,address ...
//
// Wires the fuzzer to the real decode_client() path via next_frame():
//   1. next_frame() parses the raw bytes into a FrameView (or returns an
//      error / nullopt for incomplete/malformed input — neither is a crash).
//   2. A Packet is constructed from the FrameView and passed to
//      decode_client().  Any error returned by decode_client() is silently
//      discarded; the invariant is "no crash on any input".

#include "core/bytes.hpp"
#include "protocol/bnet/codec.hpp"
#include "protocol/common/next_frame.hpp"

#include <cstddef>
#include <cstdint>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    // Wrap the raw fuzzer buffer as a core::ByteView using the project helper.
    const core::ByteView buf = core::as_byte_view(data, size);

    // Step 1: attempt to parse one complete BNet frame.
    //   - Err(DecodeError)  → malformed stream; not a crash.
    //   - Ok(nullopt)       → incomplete frame; not a crash.
    //   - Ok(FrameView)     → one full frame ready to decode.
    auto frame_result = pvpgn::protocol::common::next_frame(buf);
    if (!frame_result) return 0;          // decode error — not a crash
    auto& maybe_frame = frame_result.value();
    if (!maybe_frame) return 0;           // incomplete frame — not a crash

    const pvpgn::protocol::common::FrameView& fv = *maybe_frame;

    // Step 2: reconstruct a Packet from the FrameView so we can call
    // decode_client().  BnetHeader fields are already validated by
    // next_frame() / parse_packet(), so we just copy them out.
    pvpgn::protocol::BnetHeader hdr;
    hdr.marker = pvpgn::protocol::kBnetMarker;
    hdr.code   = fv.opcode;
    // size = header (4 bytes) + payload length
    hdr.size   = static_cast<std::uint16_t>(
        pvpgn::protocol::BnetHeader::kSize + fv.payload.size());

    pvpgn::protocol::Packet pkt;
    pkt.header  = hdr;
    pkt.payload = fv.payload;

    // Step 3: try to decode as a client → server message.
    // Any error (UnknownOpcode, OutOfRange, etc.) is intentionally ignored;
    // the only invariant we enforce is "no crash / no UB".
    (void)pvpgn::protocol::bnet::decode_client(pkt);

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
