// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/ladder/recompute_ladder.hpp"

#include <algorithm>
#include <vector>

#include "domain/ladder/ports.hpp"
#include "domain/ladder/ladder.hpp"

namespace pvpgn::application::ladder {

core::Status<RecomputeLadderResult>
RecomputeLadder::execute(RecomputeLadderCommand cmd) const {
    // 1. Validate
    if (cmd.ladder_id.empty()) {
        return core::fail(core::Error{
            core::StatusCode::InvalidArgument, "ladder_id must not be empty"});
    }

    // 2. Fetch all entries via get_top_n with a large cap
    constexpr uint32_t kMaxEntries = 100'000u;
    auto top_result = ladder_.get_top_n(kMaxEntries);
    if (!top_result) {
        return core::fail(top_result.error());
    }

    auto& entries = top_result.value();
    if (entries.empty()) {
        return RecomputeLadderResult{0};
    }

    // 3. Sort by rating descending (ties broken by wins descending),
    //    matching the original rank-bearing ladder `ladder_sort_highestrated`
    //    (src/bnetd/ladder.cpp): primary = rating, secondary = wins.
    std::stable_sort(entries.begin(), entries.end(),
        [](const domain::ladder::LadderEntry& a,
           const domain::ladder::LadderEntry& b) {
            if (a.rating != b.rating) return a.rating > b.rating;
            return a.wins > b.wins;
        });

    // 4. Save updated entries back (rank is implicit by position)
    std::size_t updated = 0;
    for (const auto& entry : entries) {
        auto save_result = ladder_.save_entry(entry);
        if (!save_result) {
            return core::fail(save_result.error());
        }
        ++updated;
    }

    return RecomputeLadderResult{updated};
}

}  // namespace pvpgn::application::ladder
