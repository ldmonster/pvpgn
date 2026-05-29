// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file channel_json.hpp
/// Free function: serialise an `IChannelRepository` snapshot to a JSON array.
///
/// Extracted from `EmbeddedWebServer::get_channels_json()` so that the
/// serialisation logic can be unit-tested without a live Boost.Asio runtime.

#include <sstream>
#include <string>

#include "application/ports/channel_repository.hpp"
#include "domain/chat/channel.hpp"

namespace pvpgn::infra::webui {

/// Return a JSON array string representing all channels in @p repo.
///
/// Each element has the shape:
/// @code
/// {
///   "id":           <uint64>,
///   "name":         "<string>",
///   "topic":        "<string>",
///   "member_count": <uint64>,
///   "permanent":    <bool>
/// }
/// @endcode
///
/// Returns `"[]"` when @p repo is null or empty.
inline std::string channels_to_json(
    const application::ports::IChannelRepository* repo)
{
    if (!repo) {
        return "[]";
    }

    // Helper: escape a string for JSON (handles " and \)
    auto escape = [](const std::string& s) -> std::string {
        std::string out;
        out.reserve(s.size());
        for (char c : s) {
            if (c == '"')       { out += "\\\""; }
            else if (c == '\\') { out += "\\\\"; }
            else                { out += c; }
        }
        return out;
    };

    std::ostringstream oss;
    oss << "[";
    bool first = true;
    repo->forEach([&](const domain::chat::Channel& ch) {
        if (!first) {
            oss << ",";
        }
        first = false;

        const bool permanent =
            ch.policy().flags.has(domain::chat::ChannelFlag::Permanent);

        oss << "{"
            << "\"id\":"           << ch.id().value()   << ","
            << "\"name\":\""       << escape(ch.name()) << "\","
            << "\"topic\":\""      << escape(ch.topic()) << "\","
            << "\"member_count\":" << ch.member_count() << ","
            << "\"permanent\":"    << (permanent ? "true" : "false")
            << "}";
        return true;  // continue iteration
    });
    oss << "]";
    return oss.str();
}

}  // namespace pvpgn::infra::webui
