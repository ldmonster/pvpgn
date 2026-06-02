// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file in_memory_mail_store.hpp
/// In-memory fake for IMailStore.
/// Suitable for tests and development/CI composition roots.

#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "domain/social/ports.hpp"
#include "core/result.hpp"


namespace pvpgn::infra::inmemory {

class InMemoryMailStore final
    : public domain::social::IMailStore {
public:
    core::Status<>
    send(domain::social::MailMessage msg) override {
        std::unique_lock lock(mutex_);
        inboxes_[msg.to].push_back(std::move(msg));
        return core::ok();
    }

    core::Result<std::vector<domain::social::MailMessage>>
    inbox(std::string_view account_name) override {
        std::shared_lock lock(mutex_);
        auto it = inboxes_.find(std::string{account_name});
        if (it == inboxes_.end()) {
            return std::vector<domain::social::MailMessage>{};
        }
        return it->second;
    }

    core::Status<>
    delete_message(std::string_view account_name, std::size_t index) override {
        std::unique_lock lock(mutex_);
        auto it = inboxes_.find(std::string{account_name});
        if (it == inboxes_.end() || index >= it->second.size()) {
            return core::fail(core::Error{
                core::StatusCode::NotFound, "mail: account or index not found"});
        }
        it->second.erase(it->second.begin() + static_cast<std::ptrdiff_t>(index));
        return core::ok();
    }

private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string,
                       std::vector<domain::social::MailMessage>> inboxes_;
};

}  // namespace pvpgn::infra::inmemory
