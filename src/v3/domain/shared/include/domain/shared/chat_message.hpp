// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file chat_message.hpp
/// Validated chat-message body (UTF-8, length-bounded). Pure value
/// object — encoding / line-stripping happen at the protocol boundary;
/// the aggregate just sees a clean payload.

#include <string>
#include <string_view>
#include <utility>

#include "core/error.hpp"
#include "core/result.hpp"

namespace pvpgn::domain {

class ChatMessage {
public:
    /// Battle.net text packets are 1..223 bytes after stripping the
    /// trailing NUL; we keep that as the upper bound.
    static constexpr std::size_t kMaxBytes = 223;

    static core::Result<ChatMessage> create(std::string_view body) {
        if (body.empty()) {
            return core::fail(core::Error{
                core::StatusCode::InvalidArgument, "chat message empty"});
        }
        if (body.size() > kMaxBytes) {
            return core::fail(core::Error{
                core::StatusCode::InvalidArgument, "chat message too long"});
        }
        for (unsigned char c : body) {
            if (c == '\n' || c == '\r' || c == '\0') {
                return core::fail(core::Error{
                    core::StatusCode::InvalidArgument,
                    "chat message contains control character"});
            }
        }
        return ChatMessage{std::string{body}};
    }

    std::string_view text() const noexcept { return body_; }
    std::size_t       size() const noexcept { return body_.size(); }

    bool operator==(const ChatMessage&) const = default;

private:
    explicit ChatMessage(std::string body) : body_(std::move(body)) {}
    std::string body_;
};

}  // namespace pvpgn::domain
