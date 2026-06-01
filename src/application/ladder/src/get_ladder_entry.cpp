// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/ladder/get_ladder_entry.hpp"

#include "domain/identity/ports.hpp"
#include "domain/ladder/ports.hpp"

namespace pvpgn::application::ladder {

core::Status<LadderEntryResult>
GetLadderEntry::execute(GetLadderEntryQuery query) const {
    // 1. Validate
    if (query.ladder_id.empty()) {
        return core::fail(core::Error{
            core::StatusCode::InvalidArgument, "ladder_id must not be empty"});
    }

    // 2. Look up the account to get its name
    auto account_result = accounts_.find_by_id(query.account_id);
    if (!account_result) {
        return core::fail(core::Error{
            core::StatusCode::NotFound, "account not found"});
    }
    const auto& account = account_result.value();

    // 3. Get the rank for this account on the ladder
    auto rank_result = ladder_.get_rank(account.name().canonical());
    if (!rank_result) {
        return core::fail(core::Error{
            core::StatusCode::NotFound,
            "account has no entry in this ladder"});
    }

    // 4. Fetch top entries to find the full stats for this account
    constexpr uint32_t kMaxEntries = 100'000u;
    auto top_result = ladder_.get_top_n(kMaxEntries);
    if (!top_result) {
        return core::fail(top_result.error());
    }

    // 5. Find the matching entry by account ID
    for (const auto& entry : top_result.value()) {
        if (entry.account == query.account_id) {
            LadderEntryResult result;
            result.account_id   = entry.account;
            result.account_name = std::string{account.name().display()};
            result.rank         = rank_result.value();
            result.wins         = entry.wins;
            result.losses       = entry.losses;
            result.disconnects  = entry.disconnects;
            result.rating       = entry.rating;
            return result;
        }
    }

    return core::fail(core::Error{
        core::StatusCode::NotFound,
        "account has no entry in this ladder"});
}

}  // namespace pvpgn::application::ladder
