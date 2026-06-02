// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file kick_connection.hpp
/// KICK_CONNECTION use-case — immediately disconnect a session.

#include <memory>
#include <string_view>

#include "domain/connection/ports.hpp"
#include "domain/identity/ports.hpp"
#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/ids.hpp"

namespace pvpgn::application::moderation {

enum class KickConnectionError : std::uint8_t {
    SessionNotFound,
    RoutingFailed,
};

class KickConnection {
public:
    KickConnection(std::shared_ptr<domain::identity::ISessionRegistry> registry,
                   std::shared_ptr<domain::connection::IMessageRouter> router)
        : registry_(registry), router_(router) {}

    core::Result<void, KickConnectionError>
    execute(domain::SessionId session_id, std::string_view reason);

private:
    std::shared_ptr<domain::identity::ISessionRegistry> registry_;
    std::shared_ptr<domain::connection::IMessageRouter> router_;
};

}  // namespace pvpgn::application::moderation
