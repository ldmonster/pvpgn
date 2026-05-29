// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file codec.hpp
/// Pure codec for the WOL (Westwood Online) IRC-like text protocol.
///
/// WOL is a line-oriented (\r\n terminated) text protocol. This codec
/// operates at the *line* level — it expects a single, already-framed
/// line (without the trailing \r\n) and returns a typed message struct.
///
/// Framing (splitting the byte stream into lines) is handled by the FSM
/// (`WolFsm`) which accumulates bytes and calls the codec per complete line.
///
/// ### Decode
///   `decode_client(line)` — parse one client → server line.
///   Returns `core::Result<ClientMessage, common::DecodeError>`.
///   - `DecodeError::Truncated`      — empty line
///   - `DecodeError::UnknownOpcode`  — command not in the known set
///   - `DecodeError::MalformedString`— required parameter missing/malformed
///
/// ### Encode
///   `encode_server(msg)` — serialise a server → client message to a
///   CRLF-terminated string ready for the wire.

#include <string>

#include "core/result.hpp"
#include "protocol/common/decode_error.hpp"
#include "protocol/wol/messages.hpp"

namespace pvpgn::protocol::wol {

// ---------------------------------------------------------------------------
// Decode
// ---------------------------------------------------------------------------

/// Decode one client → server WOL line (no trailing \r\n).
///
/// @param line  A single, complete, CRLF-stripped line from the client.
/// @return      A typed `ClientMessage` on success, or a `DecodeError`.
[[nodiscard]] core::Result<ClientMessage, common::DecodeError>
decode_client(std::string_view line);

// ---------------------------------------------------------------------------
// Encode
// ---------------------------------------------------------------------------

/// Encode a server → client `NumericReply` to a CRLF-terminated wire string.
[[nodiscard]] std::string encode_server(const NumericReply& msg);

/// Encode a server → client `RawLine` to a CRLF-terminated wire string.
[[nodiscard]] std::string encode_server(const RawLine& msg);

/// Encode any `ServerMessage` variant to a CRLF-terminated wire string.
[[nodiscard]] std::string encode_server(const ServerMessage& msg);

}  // namespace pvpgn::protocol::wol
