// SPDX-License-Identifier: GPL-2.0-or-later
#include "domain/realm/character_list.hpp"

#include <algorithm>
#include <utility>

namespace pvpgn::domain::realm {

CharacterList::CharacterList(std::string account_name, std::size_t max_capacity)
    : account_name_(std::move(account_name))
    , max_capacity_(max_capacity)
{
}

// ---------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------

core::Result<const Character*, core::Error>
CharacterList::find(std::string_view char_name) const noexcept {
    for (const auto& c : chars_) {
        if (c.id().char_name == char_name) {
            return &c;
        }
    }
    return core::fail<core::Error>(
        core::make_error(core::StatusCode::NotFound,
                         "Character not found: " + std::string(char_name)));
}

std::vector<const Character*> CharacterList::list(SortMode mode) const {
    std::vector<const Character*> view;
    view.reserve(chars_.size());
    for (const auto& c : chars_) {
        view.push_back(&c);
    }

    switch (mode) {
        case SortMode::by_name:
            std::sort(view.begin(), view.end(),
                      [](const Character* a, const Character* b) {
                          return a->id().char_name < b->id().char_name;
                      });
            break;

        case SortMode::by_level:
            std::sort(view.begin(), view.end(),
                      [](const Character* a, const Character* b) {
                          return a->stats().level > b->stats().level; // descending
                      });
            break;

        case SortMode::by_creation_time:
            std::sort(view.begin(), view.end(),
                      [](const Character* a, const Character* b) {
                          return a->created_at() < b->created_at(); // oldest first
                      });
            break;

        case SortMode::by_last_played:
            std::sort(view.begin(), view.end(),
                      [](const Character* a, const Character* b) {
                          return a->last_played() > b->last_played(); // most recent first
                      });
            break;
    }

    return view;
}

// ---------------------------------------------------------------------------
// Mutations
// ---------------------------------------------------------------------------

core::Result<void, core::Error> CharacterList::add(Character character) {
    if (is_full()) {
        return core::fail<core::Error>(
            core::make_error(core::StatusCode::ResourceExhausted,
                             "Character list is full for account: " + account_name_));
    }

    // Check for duplicate name
    for (const auto& c : chars_) {
        if (c.id().char_name == character.id().char_name) {
            return core::fail<core::Error>(
                core::make_error(core::StatusCode::AlreadyExists,
                                 "Character already exists: " + character.id().char_name));
        }
    }

    chars_.push_back(std::move(character));
    return core::Result<void, core::Error>{};
}

core::Result<void, core::Error> CharacterList::remove(std::string_view char_name) {
    auto it = std::find_if(chars_.begin(), chars_.end(),
                           [char_name](const Character& c) {
                               return c.id().char_name == char_name;
                           });

    if (it == chars_.end()) {
        return core::fail<core::Error>(
            core::make_error(core::StatusCode::NotFound,
                             "Character not found: " + std::string(char_name)));
    }

    chars_.erase(it);
    return core::Result<void, core::Error>{};
}

} // namespace pvpgn::domain::realm
