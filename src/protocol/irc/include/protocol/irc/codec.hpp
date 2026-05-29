// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file codec.hpp
/// Pure IRC codec (RFC 1459 framing + Battle.net IRC dialect).
///
/// The codec is split into:
///   * `try_parse_line(buf)`  — streaming framer; finds the first CRLF
///     terminated line in `buf`, returns it as a view + consumed count,
///     or `NeedMore` (`OutOfRange`) when no full line is buffered yet.
///   * `decode(line)`         — tokenise one already-framed line into
///     a `Message`. Commands are upper-cased to mimic IRC case folding.
///   * `encode(msg)` / `encode(msg, out)` — emit a CRLF-terminated line
///     to a `Writer` (or return it as a `std::string`).
///
/// Strictness:
///   * The framer accepts both `\r\n` and bare `\n` (legacy clients).
///   * Empty lines are rejected with `InvalidArgument`.
///   * A trailing param containing spaces *must* be prefixed with ':'.
///   * No domain-level validation (channel name shape, etc.) — that's
///     the FSM's job.

#include <cstddef>
#include <string>
#include <string_view>

#include "core/bytes.hpp"
#include "core/result.hpp"
#include "protocol/common/writer.hpp"
#include "protocol/irc/message.hpp"

namespace pvpgn::protocol::irc {

struct FramedLine {
    std::string_view line;       ///< view into the input buffer (no CRLF)
    std::size_t      consumed;   ///< bytes to drop from the front of the rx buffer
};

/// Streaming framer. Returns the next CRLF-terminated line or
/// `OutOfRange` if not enough bytes are buffered yet.
core::Result<FramedLine> try_parse_line(std::string_view buf) noexcept;

/// Tokenise one framed line (no CRLF) into a Message.
core::Result<Message> decode(std::string_view line);

/// Emit `msg` plus CRLF into `w`.
core::Status<> encode(Writer& w, const Message& msg);

/// Convenience overload that returns the encoded string (no CRLF
/// trimming — caller gets exactly the wire bytes including CRLF).
std::string encode_to_string(const Message& msg);

}  // namespace pvpgn::protocol::irc
