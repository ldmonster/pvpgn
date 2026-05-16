#include <catch2/catch_test_macros.hpp>
#include "application/realm/character_lock.hpp"
#include "domain/realm/character.hpp"
#include "core/result.hpp"
#include <memory>
#include <map>

namespace pvpgn::application::realm {

// Mock repository for testing
class MockCharacterRepository : public ICharacterRepository {
public:
    std::map<std::string, domain::realm::Character> storage;
    
    core::Result<domain::realm::Character, core::Error>
    find(const domain::realm::CharacterId& id) override {
        std::string key = id.account_name + ":" + id.char_name;
        auto it = storage.find(key);
        if (it == storage.end()) {
            return core::fail(
                core::make_error(core::StatusCode::NotFound, "Character not found")
            );
        }
        return core::Result<domain::realm::Character, core::Error>(it->second);
    }
    
    core::Result<void, core::Error>
    save(const domain::realm::Character& character) override {
        std::string key = character.id().account_name + ":" + character.id().char_name;
        storage.insert_or_assign(key, character);
        return core::Result<void, core::Error>();
    }
    
    core::Result<std::vector<domain::realm::Character>, core::Error>
    list_for_account(std::string_view account_name) override {
        std::vector<domain::realm::Character> result;
        std::string prefix = std::string(account_name) + ":";
        for (auto& [key, character] : storage) {
            if (key.find(prefix) == 0) {
                result.push_back(character);
            }
        }
        return core::Result<std::vector<domain::realm::Character>, core::Error>(result);
    }
};

TEST_CASE("CharacterLockUseCase - lock character", "[application][realm]") {
    MockCharacterRepository repo;
    
    // Setup: Create a character
    domain::realm::CharacterId id{"player1", "Barbarian"};
    domain::realm::CharacterStats stats;
    stats.char_class = domain::realm::CharacterClass::barbarian;
    domain::realm::Character character(id, stats);
    
    std::string key = "player1:Barbarian";
    repo.storage.insert_or_assign(key, character);
    
    CharacterLockUseCase use_case(repo);
    
    // Test: Lock the character
    LockCharacterCommand cmd{"player1", "Barbarian", "gs1.example.com"};
    auto result = use_case.lock(cmd);
    
    REQUIRE(result);
    
    // Verify: Character is locked
    auto find_result = repo.find(id);
    REQUIRE(find_result);
    auto locked_char = std::move(find_result).value();
    CHECK(locked_char.is_locked());
    CHECK(locked_char.locked_by().value() == "gs1.example.com");
}

TEST_CASE("CharacterLockUseCase - unlock character", "[application][realm]") {
    MockCharacterRepository repo;
    
    // Setup: Create a locked character
    domain::realm::CharacterId id{"player1", "Sorceress"};
    domain::realm::CharacterStats stats;
    stats.char_class = domain::realm::CharacterClass::sorceress;
    domain::realm::Character character(id, stats);
    auto lock_result = character.lock("gs1.example.com");
    REQUIRE(lock_result);
    
    std::string key = "player1:Sorceress";
    repo.storage.insert_or_assign(key, character);
    
    CharacterLockUseCase use_case(repo);
    
    // Test: Unlock the character
    UnlockCharacterCommand cmd{"player1", "Sorceress", "gs1.example.com"};
    auto result = use_case.unlock(cmd);
    
    REQUIRE(result);
    
    // Verify: Character is unlocked
    auto find_result = repo.find(id);
    REQUIRE(find_result);
    auto unlocked_char = std::move(find_result).value();
    CHECK_FALSE(unlocked_char.is_locked());
}

TEST_CASE("CharacterLockUseCase - is_locked returns true for locked character", "[application][realm]") {
    MockCharacterRepository repo;
    
    // Setup: Create a locked character
    domain::realm::CharacterId id{"player1", "Paladin"};
    domain::realm::CharacterStats stats;
    stats.char_class = domain::realm::CharacterClass::paladin;
    domain::realm::Character character(id, stats);
    auto lock_result = character.lock("gs2.example.com");
    REQUIRE(lock_result);
    
    std::string key = "player1:Paladin";
    repo.storage.insert_or_assign(key, character);
    
    CharacterLockUseCase use_case(repo);
    
    // Test: Check if locked
    auto result = use_case.is_locked(id);
    
    REQUIRE(result);
    CHECK(result.value() == true);
}

TEST_CASE("CharacterLockUseCase - is_locked returns false for unlocked character", "[application][realm]") {
    MockCharacterRepository repo;
    
    // Setup: Create an unlocked character
    domain::realm::CharacterId id{"player1", "Amazon"};
    domain::realm::CharacterStats stats;
    stats.char_class = domain::realm::CharacterClass::amazon;
    domain::realm::Character character(id, stats);
    
    std::string key = "player1:Amazon";
    repo.storage.insert_or_assign(key, character);
    
    CharacterLockUseCase use_case(repo);
    
    // Test: Check if locked
    auto result = use_case.is_locked(id);
    
    REQUIRE(result);
    CHECK(result.value() == false);
}

TEST_CASE("CharacterLockUseCase - lock non-existent character fails", "[application][realm]") {
    MockCharacterRepository repo;
    CharacterLockUseCase use_case(repo);
    
    // Test: Try to lock non-existent character
    LockCharacterCommand cmd{"player1", "NonExistent", "gs1.example.com"};
    auto result = use_case.lock(cmd);
    
    CHECK_FALSE(result);
}

TEST_CASE("CharacterLockUseCase - unlock non-existent character fails", "[application][realm]") {
    MockCharacterRepository repo;
    CharacterLockUseCase use_case(repo);
    
    // Test: Try to unlock non-existent character
    UnlockCharacterCommand cmd{"player1", "NonExistent", "gs1.example.com"};
    auto result = use_case.unlock(cmd);
    
    CHECK_FALSE(result);
}

TEST_CASE("CharacterLockUseCase - unlock with wrong GS fails", "[application][realm]") {
    MockCharacterRepository repo;
    
    // Setup: Create a character locked by gs1
    domain::realm::CharacterId id{"player1", "Druid"};
    domain::realm::CharacterStats stats;
    stats.char_class = domain::realm::CharacterClass::druid;
    domain::realm::Character character(id, stats);
    auto lock_result = character.lock("gs1.example.com");
    REQUIRE(lock_result);
    
    std::string key = "player1:Druid";
    repo.storage.insert_or_assign(key, character);
    
    CharacterLockUseCase use_case(repo);
    
    // Test: Try to unlock with different GS
    UnlockCharacterCommand cmd{"player1", "Druid", "gs2.example.com"};
    auto result = use_case.unlock(cmd);
    
    CHECK_FALSE(result);
}

} // namespace pvpgn::application::realm
