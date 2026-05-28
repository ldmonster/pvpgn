// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/ladder/get_ladder_page.hpp"

#include <string>

#include "application/ports/account_repository.hpp"
#include "application/ports/ladder_repository.hpp"

namespace pvpgn::application::ladder {

core::Status<LadderPage>
GetLadderPage::execute(GetLadderPageQuery query) const {
    // 1. Validate
    if (query.page < 1) {
        return core::fail(core::Error{
            core::StatusCode::InvalidArgument, "page must be >= 1"});
    }
    if (query.page_size < 1) {
        return core::fail(core::Error{
            core::StatusCode::InvalidArgument, "page_size must be >= 1"});
    }
    if (query.page_size > 100) {
        return core::fail(core::Error{
            core::StatusCode::InvalidArgument, "page_size must be <= 100"});
    }
    if (query.ladder_id.empty()) {
        return core::fail(core::Error{
            core::StatusCode::InvalidArgument, "ladder_id must not be empty"});
    }

    // 2. Fetch all entries
    constexpr uint32_t kMaxEntries = 100'000u;
    auto top_result = ladder_.get_top_n(kMaxEntries);
    if (!top_result) {
        return core::fail(top_result.error());
    }

    const auto& all_entries = top_result.value();
    const auto  total       = static_cast<std::uint32_t>(all_entries.size());

    // 3. Compute slice bounds (0-based)
    const std::uint32_t offset = (query.page - 1) * query.page_size;

    LadderPage page_result;
    page_result.page          = query.page;
    page_result.page_size     = query.page_size;
    page_result.total_entries = total;

    if (offset >= total) {
        // Page beyond the end — return empty entries list
        return page_result;
    }

    const std::uint32_t end = std::min(offset + query.page_size, total);

    // 4. Build result entries for the slice
    for (std::uint32_t i = offset; i < end; ++i) {
        const auto& entry = all_entries[i];

        // Look up account name
        auto account_result = accounts_.find_by_id(entry.account.value());
        std::string name;
        if (account_result) {
            name = std::string{account_result.value().name().display()};
        }

        // Rank is 1-based position in the sorted list
        const std::uint32_t rank = i + 1u;

        LadderEntryResult r;
        r.account_id   = entry.account;
        r.account_name = std::move(name);
        r.rank         = rank;
        r.wins         = entry.wins;
        r.losses       = entry.losses;
        r.disconnects  = entry.disconnects;
        r.rating       = entry.rating;
        page_result.entries.push_back(std::move(r));
    }

    return page_result;
}

}  // namespace pvpgn::application::ladder
