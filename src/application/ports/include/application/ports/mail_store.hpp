// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file mail_store.hpp
/// Port: persistent mail store for inter-account messaging.

#include <string>
#include <vector>

#include "core/clock.hpp"
#include "core/result.hpp"

namespace pvpgn::application::ports {

/// A single mail message stored in the system.
struct MailMessage {
    std::string      to;       ///< Recipient account name or e-mail address.
    std::string      subject;
    std::string      body;
    core::SystemTime sent_at;
};

/// Port: mail store interface for hexagonal architecture.
/// Implementations provide persistence backends (InMemory, SQLite, file, etc.).
class IMailStore {
public:
    virtual ~IMailStore() = default;

    /// Deliver a message (appends to the recipient's inbox).
    virtual core::Status<>
    send(MailMessage msg) = 0;

    /// Return all messages in the named account's inbox.
    virtual core::Result<std::vector<MailMessage>>
    inbox(std::string_view account_name) = 0;

    /// Delete the message at position `index` in the named account's inbox.
    /// Returns `NotFound` if the account or index does not exist.
    virtual core::Status<>
    delete_message(std::string_view account_name, std::size_t index) = 0;
};

}  // namespace pvpgn::application::ports
