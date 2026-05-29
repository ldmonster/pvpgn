// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file replay.hpp
/// Protocol replay harness — feeds a captured byte stream through a
/// codec's decode function and collects the resulting message sequence.
///
/// The harness is intentionally generic. A protocol provides:
///   * a `Decoded` type (typically a `std::variant`)
///   * a `decode(const Packet&) -> core::Result<Decoded>` free function
///
/// Use cases:
///   * Golden tests: capture one BNet login session once, replay it on
///     every CI run. Catches accidental wire-format regressions.
///   * Shadow-mode parity: feed the same bytes through legacy `handle_*`
///     and new codec, compare.
///   * Fuzz harnesses: libFuzzer calls `replay()` on its input buffer.
///
/// All errors propagate via `core::Result`. The harness never throws.

#include <cstddef>
#include <utility>
#include <vector>

#include "core/bytes.hpp"
#include "core/error.hpp"
#include "core/result.hpp"
#include "protocol/common/packet.hpp"

namespace pvpgn::protocol {

struct ReplayStats {
    std::size_t packets_decoded = 0;
    std::size_t bytes_consumed  = 0;
    std::size_t bytes_trailing  = 0;  ///< partial packet at the tail
};

template <class DecodedT>
struct ReplayResult {
    std::vector<DecodedT> messages;
    ReplayStats           stats;
};

/// Replay a captured byte stream through `decode`. Stops at the first
/// hard error (anything except a trailing `OutOfRange`, which signals a
/// partial last packet and is exposed via `stats.bytes_trailing`).
template <class DecodedT, class DecodeFn>
core::Result<ReplayResult<DecodedT>>
replay(core::ByteView stream, DecodeFn decode) {
    ReplayResult<DecodedT> r;
    core::ByteView cursor = stream;
    while (!cursor.empty()) {
        auto fp = parse_packet(cursor);
        if (!fp) {
            // OutOfRange at the end of the stream → partial last packet,
            // which is normal for live captures. Anything else is fatal.
            if (fp.error().code() == core::StatusCode::OutOfRange) {
                r.stats.bytes_trailing = cursor.size();
                break;
            }
            return core::fail(fp.error());
        }
        auto msg = decode(fp.value().packet);
        if (!msg) return core::fail(msg.error());
        r.messages.push_back(std::move(msg).value());
        cursor = cursor.subspan(fp.value().consumed);
        r.stats.packets_decoded += 1;
        r.stats.bytes_consumed  += fp.value().consumed;
    }
    return r;
}

}  // namespace pvpgn::protocol
