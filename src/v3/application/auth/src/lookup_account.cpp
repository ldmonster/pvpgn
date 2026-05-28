// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/auth/lookup_account.hpp"

#include "core/error.hpp"

namespace pvpgn::application::auth {

core::Status<LookupAccountResult>
LookupAccountByName::execute(std::string_view name) const {
    // 1. Look up the account by name in the repository.
    auto found = repo_.find_by_name(name);
    if (!found) {
        return core::fail(core::Error{core::StatusCode::NotFound,
                                      "account not found"});
    }
    const auto& account = found.value();

    // 2. Check whether the account currently has an active session.
    const bool online = sessions_.session_for(account.id()).has_value();

    // 3. Assemble and return the result.
    return LookupAccountResult{
        account.id(),
        std::string{account.name().canonical()},
        account.is_locked(),
        online,
    };
}

}  // namespace pvpgn::application::auth
