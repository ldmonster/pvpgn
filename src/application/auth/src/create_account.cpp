// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/auth/create_account.hpp"

#include <cstdint>
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

    // 3. Allocate the new account id as (max existing id) + 1, or 1 if the
    // repository is empty. This mirrors the original server's strictly
    // sequential, monotonic, never-reused uid allocation (maxuserid + 1,
    // seeded from storage; first-ever account gets uid 1 — see
    // bnetd/account.cpp:157,686 in the original).
    //
    // Rationale: the previous implementation derived the id from a 31-bit
    // hash of the canonical username. That was non-portable (std::hash is
    // implementation-defined) and, critically, two distinct usernames could
    // collide on the same id. Because both account repositories key on id
    // (SQL: ON CONFLICT(id) DO UPDATE; in-memory: by_id_[id] = ...), a
    // collision would SILENTLY OVERWRITE an existing, differently-named
    // account — data loss / account takeover. max+1 guarantees a fresh,
    // unique id and never clobbers an existing row.
    //
    // NOTE: a persistent, atomic monotonic counter (an IAccountIdAllocator
    // port seeded once from max(uid) at startup) would be the
    // production-grade approach. max+1 computed via forEach is O(n) per
    // create but yields the same observable result (sequential ids starting
    // at 1) and is correct at the current scale.
    std::uint32_t max_id = 0;
    accounts_.forEach([&](const domain::identity::Account& acc) {
        if (acc.id().value() > max_id) {
            max_id = acc.id().value();
        }
        return true;  // keep iterating over every account
    });
    auto new_id = domain::AccountId(max_id + 1);

    // 4. Create the account aggregate. This emits AccountCreated.
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
