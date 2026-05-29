// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file realm.hpp
/// `realm::Realm` aggregate + `Character` value object — Diablo II
/// realm catalog. Mirrors legacy `bnetd/realm.cpp` /
/// `d2cs/d2charfile.cpp` invariants (unique character name per realm,
/// case-insensitive comparison; 16-char name limit).

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/events.hpp"
#include "domain/shared/ids.hpp"

namespace pvpgn::domain::realm {

struct Character {
    AccountId   owner;
    std::string name;
    std::string class_code;  // legacy `D2_CHAR_CLASS_*` string
};

class Realm {
public:
    static constexpr std::size_t kMaxNameLen = 16;

    static core::Result<Realm>
    create(std::uint32_t id, std::string name, std::string description) {
        if (name.empty() || name.size() > 32) {
            return core::fail(core::Error{
                core::StatusCode::InvalidArgument, "realm name 1..32 chars"});
        }
        Realm r{id, std::move(name), std::move(description)};
        r.events_.push_back(events::RealmRegistered{r.id_, r.name_});
        return r;
    }

    std::uint32_t                  id()          const noexcept { return id_; }
    const std::string&             name()        const noexcept { return name_; }
    const std::string&             description() const noexcept { return desc_; }
    bool                           active()      const noexcept { return active_; }
    const std::vector<Character>&  characters()  const noexcept { return characters_; }
    std::size_t                    character_count() const noexcept { return characters_.size(); }

    enum class CreateOutcome : std::uint8_t {
        Created, NameTaken, BadName,
    };

    CreateOutcome create_character(AccountId owner, std::string name,
                                   std::string class_code) {
        if (name.empty() || name.size() > kMaxNameLen) return CreateOutcome::BadName;
        if (find_(name) != characters_.end())          return CreateOutcome::NameTaken;
        characters_.push_back({owner, name, std::move(class_code)});
        events_.push_back(events::CharacterCreated{id_, owner, name});
        return CreateOutcome::Created;
    }

    bool delete_character(AccountId owner, const std::string& name) {
        auto it = find_(name);
        if (it == characters_.end() || it->owner != owner) return false;
        events_.push_back(events::CharacterDeleted{id_, owner, name});
        characters_.erase(it);
        return true;
    }

    void unregister() {
        if (!active_) return;
        active_ = false;
        events_.push_back(events::RealmUnregistered{id_});
    }

    std::vector<events::DomainEvent> drain_events() {
        return std::exchange(events_, {});
    }

private:
    Realm(std::uint32_t id, std::string name, std::string desc)
        : id_(id), name_(std::move(name)), desc_(std::move(desc)) {}

    /// Case-insensitive character-name lookup.
    std::vector<Character>::iterator find_(const std::string& name) {
        return std::find_if(characters_.begin(), characters_.end(),
            [&](const Character& c) { return iequals_(c.name, name); });
    }

    static bool iequals_(const std::string& a, const std::string& b) noexcept {
        if (a.size() != b.size()) return false;
        for (std::size_t i = 0; i < a.size(); ++i) {
            if (std::tolower(static_cast<unsigned char>(a[i])) !=
                std::tolower(static_cast<unsigned char>(b[i]))) return false;
        }
        return true;
    }

    std::uint32_t                       id_;
    std::string                         name_;
    std::string                         desc_;
    bool                                active_ = true;
    std::vector<Character>              characters_;
    std::vector<events::DomainEvent>    events_;
};

}  // namespace pvpgn::domain::realm
