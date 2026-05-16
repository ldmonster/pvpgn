#pragma once
#include <string>
#include <cstdint>
#include <chrono>
#include <optional>
#include "core/result.hpp"

namespace pvpgn::domain::realm {

enum class CharacterClass : uint8_t {
    amazon = 0, necromancer, paladin, barbarian, sorceress, druid, assassin
};

enum class CharacterExpansion : uint8_t { classic = 0, lod = 1 };

enum class CharacterHardcore : uint8_t { softcore = 0, hardcore = 1 };

struct CharacterId {
    std::string account_name;
    std::string char_name;
};

struct CharacterStats {
    uint32_t level = 1;
    uint32_t experience = 0;
    CharacterClass char_class = CharacterClass::amazon;
    CharacterExpansion expansion = CharacterExpansion::classic;
    CharacterHardcore hardcore = CharacterHardcore::softcore;
    bool dead = false;
    uint32_t strength = 0;
    uint32_t dexterity = 0;
    uint32_t vitality = 0;
    uint32_t energy = 0;
};

class Character {
public:
    Character(CharacterId id, CharacterStats stats);
    
    const CharacterId& id() const noexcept { return id_; }
    const CharacterStats& stats() const noexcept { return stats_; }
    
    bool is_locked() const noexcept { return locked_by_gs_.has_value(); }
    const std::optional<std::string>& locked_by() const noexcept { return locked_by_gs_; }
    
    core::Result<void, core::Error> lock(std::string gs_address);
    core::Result<void, core::Error> unlock(std::string_view gs_address);
    
    std::chrono::system_clock::time_point created_at() const noexcept { return created_at_; }
    std::chrono::system_clock::time_point last_played() const noexcept { return last_played_; }
    void touch() { last_played_ = std::chrono::system_clock::now(); }

private:
    CharacterId id_;
    CharacterStats stats_;
    std::optional<std::string> locked_by_gs_;
    std::chrono::system_clock::time_point created_at_;
    std::chrono::system_clock::time_point last_played_;
};

} // namespace pvpgn::domain::realm
