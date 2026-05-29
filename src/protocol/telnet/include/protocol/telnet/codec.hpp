// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file codec.hpp
/// Telnet admin-interface line codec.
///
/// PvPGN's admin telnet is a plain CRLF/LF-terminated request/response
/// channel — no IAC negotiation, no subcommands. This codec exposes
/// only the framer and a trivial "command + args" tokeniser.

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "core/bytes.hpp"
#include "core/result.hpp"
#include "protocol/common/writer.hpp"

namespace pvpgn::protocol::telnet {

struct FramedLine {
    std::string_view line;
    std::size_t      consumed = 0;
};

/// Streaming framer: find the next CRLF (or bare LF) and return the
/// line + bytes-to-consume. `OutOfRange` ⇒ need more bytes.
core::Result<FramedLine> try_parse_line(std::string_view buf) noexcept;

struct Command {
    std::string              verb;
    std::vector<std::string> args;
    bool operator==(const Command&) const = default;
};

/// Tokenise a framed line into `verb` + whitespace-separated args.
/// Leading/trailing whitespace is trimmed; empty lines yield an empty
/// `verb`.
Command tokenise(std::string_view line);

/// Append a CRLF-terminated reply line to `w`.
void write_line(Writer& w, std::string_view line);

}  // namespace pvpgn::protocol::telnet
