// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file get_ladder_entry.hpp
/// GET_LADDER_ENTRY use-case — fetch a single account's ladder entry.

#include <cstdint>
#include <string>

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/ids.hpp"

namespace pvpgn::application::ladder {

struct GetLadderEntryQuery {
    std::string        ladder_id;
    domain::AccountId  account_id;
};

struct LadderEntryResult {
    domain::AccountId  account_id;
    std::string        account_name;
    std::uint32_t      rank;
    std::uint32_t      wins;
    std::uint32_t      losses;
    std::uint32_t      disconnects;
    std::int32_t       rating;
};

class GetLadderEntry {
public:
    explicit GetLadderEntry(ports::ILadderRepository& ladder,
                            ports::IAccountRepository& accounts)
        : ladder_(ladder), accounts_(accounts) {}

    /// Returns InvalidArgument if ladder_id is empty.
    /// Returns NotFound if the account has no entry in this ladder.
    [[nodiscard]] core::Status<LadderEntryResult>
    execute(GetLadderEntryQuery query) const;

private:
    ports::ILadderRepository&  ladder_;
    ports::IAccountRepository& accounts_;
};

}  // namespace pvpgn::application::ladder
