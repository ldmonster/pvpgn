// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
//
// domain/ladder/ports.hpp — Abstract ports (interfaces) for the ladder bounded context.
// Implementations live in src/infra/<tech>/ and src/integration/<binding>/.
// Plan 05: Ports Consolidation (migrated from application/ports/)

#include <cstdint>
#include <vector>

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/ladder/ladder.hpp"
#include "domain/shared/ids.hpp"

namespace pvpgn::domain::ladder {

// ---------------------------------------------------------------------------
// ILadderRepository
// ---------------------------------------------------------------------------

class ILadderRepository {
public:
    virtual ~ILadderRepository() = default;

    ILadderRepository(const ILadderRepository&)            = delete;
    ILadderRepository& operator=(const ILadderRepository&) = delete;
    ILadderRepository(ILadderRepository&&)                 = delete;
    ILadderRepository& operator=(ILadderRepository&&)      = delete;

    // Rank is 1-based; keyed by account id to match `save_entry` /
    // `LadderEntry` (Plan 07: the legacy by-name signature could not be
    // satisfied — entries carry only an id).
    [[nodiscard]] virtual core::Result<std::uint32_t, core::Error>
    get_rank(domain::AccountId account_id) = 0;

    virtual core::Result<void, core::Error>
    save_entry(const LadderEntry& entry) = 0;

    [[nodiscard]] virtual core::Result<std::vector<LadderEntry>,
                                       core::Error>
    get_top_n(std::uint32_t n) = 0;

protected:
    ILadderRepository() = default;
};

} // namespace pvpgn::domain::ladder
