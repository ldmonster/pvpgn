// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file get_ladder_page.hpp
/// GET_LADDER_PAGE use-case — fetch a paginated slice of the ladder.

#include <cstdint>
#include <string>
#include <vector>

#include "core/error.hpp"
#include "core/result.hpp"
#include "application/ladder/get_ladder_entry.hpp"

namespace pvpgn::application::ladder {

struct GetLadderPageQuery {
    std::string    ladder_id;
    std::uint32_t  page;       ///< 1-based page number
    std::uint32_t  page_size;  ///< entries per page (max 100)
};

struct LadderPage {
    std::uint32_t               page;
    std::uint32_t               page_size;
    std::uint32_t               total_entries;
    std::vector<LadderEntryResult> entries;
};

class GetLadderPage {
public:
    explicit GetLadderPage(ports::ILadderRepository& ladder,
                           ports::IAccountRepository& accounts)
        : ladder_(ladder), accounts_(accounts) {}

    /// Returns InvalidArgument if page < 1, page_size < 1, or page_size > 100.
    [[nodiscard]] core::Status<LadderPage>
    execute(GetLadderPageQuery query) const;

private:
    ports::ILadderRepository&  ladder_;
    ports::IAccountRepository& accounts_;
};

}  // namespace pvpgn::application::ladder
