// SPDX-License-Identifier: GPL-2.0-or-later
#include "domain/ladder/d2_ladder.hpp"

#include <algorithm>
#include <utility>

namespace pvpgn::domain::ladder {

namespace {

/// Determine the maximum entry count for a given ladder type.
constexpr std::size_t max_entries_for(D2LadderType type) noexcept {
    switch (type) {
        case D2LadderType::std_overall:
        case D2LadderType::hc_overall:
        case D2LadderType::exp_std_overall:
        case D2LadderType::exp_hc_overall:
            return kMaxOverallLadderEntries;
        default:
            return kMaxClassLadderEntries;
    }
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

D2Ladder::D2Ladder(D2LadderType type)
    : type_(type)
    , max_entries_(max_entries_for(type))
{
}

// ---------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------

std::span<const D2LadderEntry> D2Ladder::top(std::size_t n) const noexcept {
    const std::size_t count = std::min(n, entries_.size());
    return {entries_.data(), count};
}

std::optional<std::size_t> D2Ladder::rank_of(std::string_view char_name) const noexcept {
    for (std::size_t i = 0; i < entries_.size(); ++i) {
        if (entries_[i].char_name == char_name) {
            return i + 1; // 1-based
        }
    }
    return std::nullopt;
}

// ---------------------------------------------------------------------------
// Mutations
// ---------------------------------------------------------------------------

core::Result<void, core::Error> D2Ladder::add(D2LadderEntry entry) {
    if (is_full()) {
        return core::fail<core::Error>(
            core::make_error(core::StatusCode::ResourceExhausted,
                             "Ladder is full"));
    }

    // Check for duplicate char_name
    for (const auto& e : entries_) {
        if (e.char_name == entry.char_name) {
            return core::fail<core::Error>(
                core::make_error(core::StatusCode::AlreadyExists,
                                 "Character already on ladder: " + entry.char_name));
        }
    }

    // Insert in sorted position (experience descending)
    auto pos = std::lower_bound(
        entries_.begin(), entries_.end(), entry,
        [](const D2LadderEntry& a, const D2LadderEntry& b) {
            return a.experience > b.experience; // descending
        });

    entries_.insert(pos, std::move(entry));
    return core::Result<void, core::Error>{};
}

core::Result<void, core::Error> D2Ladder::remove(std::string_view char_name) {
    auto it = std::find_if(entries_.begin(), entries_.end(),
                           [char_name](const D2LadderEntry& e) {
                               return e.char_name == char_name;
                           });

    if (it == entries_.end()) {
        return core::fail<core::Error>(
            core::make_error(core::StatusCode::NotFound,
                             "Character not on ladder: " + std::string(char_name)));
    }

    entries_.erase(it);
    return core::Result<void, core::Error>{};
}

} // namespace pvpgn::domain::ladder
