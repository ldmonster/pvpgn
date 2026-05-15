// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/auth/create_account.hpp"

#include <functional>

namespace pvpgn::application::auth {

CreateAccount::Result CreateAccount::execute(const CreateAccountRequest& req) {
    // 1. Check IP ban before anything else.
    auto ip_check = ip_bans_.is_banned(req.peer_ip);
    if (!ip_check) {
        return core::fail(CreateAccountError::Internal);
    }
    if (ip_check.value()) {
        return core::fail(CreateAccountError::IpBanned);
    }

    // 2. Check if username is already taken.
    auto existing = accounts_.find_by_name(req.username);
    if (existing) {
        return core::fail(CreateAccountError::UsernameTaken);
    }

    // 3. Create the account aggregate. This emits AccountCreated.
    // ID generation: use a simple hash of the canonical username to ensure
    // determinism in tests. In production, this would use a proper ID generator.
    std::string canonical_str(req.username.canonical());
    unsigned int id_value = std::hash<std::string>{}(canonical_str) & 0x7FFFFFFFu;
    auto new_id = domain::AccountId(id_value);
    auto account_result =
        domain::identity::Account::create(new_id, req.username, req.password_hash, req.locale);
    if (!account_result) {
        return core::fail(CreateAccountError::Internal);
    }
    domain::identity::Account account = account_result.value();

    // 6. Persist the new account.
    auto saved = accounts_.save(account);
    if (!saved) {
        return core::fail(CreateAccountError::PersistenceFailed);
    }

    // 7. Drain and publish all domain events.
    for (const auto& e : account.drain_events()) {
        bus_.publish(e);
    }

    return account.id();
}

}  // namespace pvpgn::application::auth
