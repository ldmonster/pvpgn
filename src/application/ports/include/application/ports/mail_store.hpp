// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file mail_store.hpp
/// Port for the per-account in-game mail box. Adapters: file-backed
/// (`infra/file`), in-memory fake (`infra/inmemory`), SQL backends.

#include "core/error.hpp"
#include "core/result.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace pvpgn::application::ports {

/// A single mail message, addressed to / from a Battle.net account.
struct MailMessage {
    std::string   from;
    std::string   to;
    std::string   subject;
    std::string   body;
    std::uint64_t timestamp = 0;  ///< unix seconds
};

/// Persistence boundary for the in-game mail subsystem.
class IMailStore {
public:
    virtual ~IMailStore() = default;

    /// Deliver `msg` to the recipient's inbox.
    virtual core::Status<> send(MailMessage msg) = 0;

    /// Read all messages currently in `account_name`'s inbox.
    /// Returns an empty vector if the account has none.
    virtual core::Result<std::vector<MailMessage>>
        inbox(std::string_view account_name) = 0;

    /// Remove the message at `index` from `account_name`'s inbox.
    virtual core::Status<>
        delete_message(std::string_view account_name, std::size_t index) = 0;
};

} // namespace pvpgn::application::ports
