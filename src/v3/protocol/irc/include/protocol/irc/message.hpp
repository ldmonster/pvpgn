// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file message.hpp
/// RFC 1459 IRC message value type — pure, allocation-light.
///
/// A line on the wire is:
///   `[":" prefix SPACE] command (SPACE param)* [SPACE ":" trailing] CRLF`
///
/// We store fields as `std::string` (owning) so the message can outlive
/// the buffer it was decoded from. PvPGN extends RFC 1459 only at the
/// command-vocabulary level — the framing/tokenisation is stock.

#include <cstdint>
#include <string>
#include <vector>

namespace pvpgn::protocol::irc {

struct Message {
    std::string              prefix;   ///< without the leading ':'
    std::string              command;  ///< uppercase, e.g. "PRIVMSG"
    std::vector<std::string> params;   ///< middle params + trailing (last)

    bool operator==(const Message&) const = default;

    /// Convenience: trailing param is the last one, if any.
    const std::string& trailing_or(const std::string& fallback) const {
        return params.empty() ? fallback : params.back();
    }
};

}  // namespace pvpgn::protocol::irc
