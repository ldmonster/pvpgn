// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file message_router.hpp
/// Application-layer port for routing encoded bytes to sessions / accounts.

#include <cstddef>
#include <span>

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/ids.hpp"

namespace pvpgn::application::ports {

class IMessageRouter {
public:
    virtual ~IMessageRouter() = default;

    IMessageRouter(const IMessageRouter&)            = delete;
    IMessageRouter& operator=(const IMessageRouter&) = delete;
    IMessageRouter(IMessageRouter&&)                 = delete;
    IMessageRouter& operator=(IMessageRouter&&)      = delete;

    virtual core::Result<void, core::Error>
    send(domain::SessionId session_id, std::span<const std::byte> bytes) = 0;

    virtual core::Result<void, core::Error>
    broadcast(std::span<const domain::SessionId> sessions,
              std::span<const std::byte> bytes) = 0;

    virtual core::Result<void, core::Error>
    send_to_account(domain::AccountId account_id,
                    std::span<const std::byte> bytes) = 0;

protected:
    IMessageRouter() = default;
};

} // namespace pvpgn::application::ports
