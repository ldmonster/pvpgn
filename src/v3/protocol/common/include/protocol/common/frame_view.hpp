// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>

#include "core/bytes.hpp"

namespace pvpgn::protocol::common {

/// A non-owning view of a single complete protocol frame (header + payload).
struct FrameView {
    core::ByteView header;   ///< Raw header bytes
    core::ByteView payload;  ///< Raw payload bytes (after header)
    std::uint8_t   opcode;   ///< Extracted opcode / message type
};

} // namespace pvpgn::protocol::common
