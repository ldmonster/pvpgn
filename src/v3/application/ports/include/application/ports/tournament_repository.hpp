// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file tournament_repository.hpp
/// Tournament repository port.
///
/// Provides read/write access to tournament metadata and standings.
/// Implementations live in `infra/` and are wired by the composition root.

#include "core/result.hpp"
#include "core/clock.hpp"
#include "domain/shared/ids.hpp"
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace pvpgn::application::ports {

/// A single participant's standing within a tournament.
struct TournamentEntry {
    domain::AccountId account_id;
    std::string       account_name;
    std::uint32_t     wins{0};
    std::uint32_t     losses{0};
    std::uint32_t     points{0};
};

/// Metadata describing a tournament.
struct TournamentInfo {
    std::string      id;           ///< e.g. "STAR_SEASON1"
    std::string      name;
    std::string      description;
    core::SystemTime starts_at;
    core::SystemTime ends_at;
    bool             is_active{false};
};

/// Port: tournament repository interface.
class ITournamentRepository {
public:
    virtual ~ITournamentRepository() = default;

    /// Find a tournament by its string identifier.
    virtual core::Status<TournamentInfo>
    find_by_id(std::string_view tournament_id) const = 0;

    /// List all currently active tournaments.
    virtual core::Status<std::vector<TournamentInfo>>
    list_active() const = 0;

    /// Retrieve the top-N standings for a tournament.
    virtual core::Status<std::vector<TournamentEntry>>
    get_standings(std::string_view tournament_id,
                  std::uint32_t    max_entries) const = 0;

    /// Record the outcome of a match (winner gains, loser loses).
    virtual core::Status<void>
    record_result(std::string_view  tournament_id,
                  domain::AccountId winner_id,
                  domain::AccountId loser_id) = 0;
};

} // namespace pvpgn::application::ports
