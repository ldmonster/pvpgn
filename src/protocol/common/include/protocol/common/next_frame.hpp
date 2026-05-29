// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file next_frame.hpp
/// `next_frame()` — a thin wrapper around `parse_packet()` that translates
/// the low-level `core::Error` taxonomy into the higher-level `DecodeError`
/// enum and returns `std::optional<FrameView>` to distinguish "not enough
/// bytes yet" (nullopt / Ok) from a genuine decode failure (Err).
///
/// Typical usage in a session read-loop:
/// @code
///   auto result = pvpgn::protocol::common::next_frame(recv_buf);
///   if (!result) { /* terminal error — close session */ }
///   if (!result.value()) { /* incomplete — wait for more data */ }
///   auto& fv = *result.value();
///   // dispatch on fv.opcode …
/// @endcode

#include <optional>

#include "core/bytes.hpp"
#include "core/error.hpp"
#include "core/result.hpp"
#include "protocol/common/decode_error.hpp"
#include "protocol/common/frame_view.hpp"
#include "protocol/common/packet.hpp"

namespace pvpgn::protocol::common {

/// Parse the next complete BNet frame from `buf`.
///
/// Returns:
///   - `Ok(nullopt)`          — buffer is too short; caller should buffer more
///                              data and retry (not an error).
///   - `Ok(FrameView{…})`     — one complete frame was parsed successfully.
///   - `Err(DecodeError::InvalidLength)` — the declared packet size is smaller
///                              than the 4-byte header (malformed stream).
///   - `Err(DecodeError::Truncated)` — the marker byte is wrong or another
///                              structural invariant is violated.
///
/// The returned `FrameView` aliases memory inside `buf`; its lifetime is
/// bounded by `buf`'s lifetime.
[[nodiscard]] inline core::Result<std::optional<FrameView>, DecodeError>
next_frame(core::ByteView buf) noexcept {
    using Ret = core::Result<std::optional<FrameView>, DecodeError>;

    auto fp = pvpgn::protocol::parse_packet(buf);
    if (!fp) {
        const auto& err = fp.error();
        // OutOfRange from parse_packet means the buffer does not yet hold a
        // complete packet — this is normal back-pressure, not an error.
        if (err.code() == core::StatusCode::OutOfRange) {
            return Ret{std::optional<FrameView>{std::nullopt}};
        }
        // InvalidArgument covers: bad marker byte, or size field < 4.
        // The size-field case maps to InvalidLength; the marker case maps to
        // Truncated (stream is corrupt / not a BNet connection).
        if (err.code() == core::StatusCode::InvalidArgument) {
            // Distinguish "size < header" from "bad marker" by re-examining
            // the raw buffer.  If we have at least 4 bytes and the first byte
            // is the BNet marker, the size field must be the problem.
            if (buf.size() >= pvpgn::protocol::BnetHeader::kSize &&
                static_cast<std::uint8_t>(buf[0]) == pvpgn::protocol::kBnetMarker) {
                return core::fail(DecodeError::InvalidLength);
            }
            return core::fail(DecodeError::Truncated);
        }
        // Any other error code is treated as a truncated / corrupt stream.
        return core::fail(DecodeError::Truncated);
    }

    const auto& framed = fp.value();
    FrameView fv;
    fv.header  = buf.subspan(0, pvpgn::protocol::BnetHeader::kSize);
    fv.payload = framed.packet.payload;
    fv.opcode  = framed.packet.header.code;
    return Ret{std::optional<FrameView>{fv}};
}

} // namespace pvpgn::protocol::common
