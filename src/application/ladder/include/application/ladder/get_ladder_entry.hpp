// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file get_ladder_entry.hpp
/// GET_LADDER_ENTRY use-case — fetch a single account's ladder entry.

#include <cstdint>
#include <string>

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/identity/ports.hpp"
#include "domain/ladder/ports.hpp"
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
    explicit GetLadderEntry(domain::ladder::ILadderRepository& ladder,
                            domain::identity::IAccountReader& accounts)
        : ladder_(ladder), accounts_(accounts) {}

    /// Returns InvalidArgument if ladder_id is empty.
    /// Returns NotFound if the account has no entry in this ladder.
    [[nodiscard]] core::Status<LadderEntryResult>
    execute(GetLadderEntryQuery query) const;

private:
    domain::ladder::ILadderRepository&    ladder_;
    domain::identity::IAccountReader& accounts_;
};

}  // namespace pvpgn::application::ladder
