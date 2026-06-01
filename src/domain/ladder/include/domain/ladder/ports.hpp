// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
//
// domain/ladder/ports.hpp — Abstract ports (interfaces) for the ladder bounded context.
// Implementations live in src/infra/<tech>/ and src/integration/<binding>/.
// Plan 05: Ports Consolidation (migrated from application/ports/)

#include <cstdint>
#include <string_view>
#include <vector>

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/ladder/ladder.hpp"

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

    [[nodiscard]] virtual core::Result<std::uint32_t, core::Error>
    get_rank(std::string_view account_name) = 0;

    virtual core::Result<void, core::Error>
    save_entry(const LadderEntry& entry) = 0;

    [[nodiscard]] virtual core::Result<std::vector<LadderEntry>,
                                       core::Error>
    get_top_n(std::uint32_t n) = 0;

protected:
    ILadderRepository() = default;
};

} // namespace pvpgn::domain::ladder
