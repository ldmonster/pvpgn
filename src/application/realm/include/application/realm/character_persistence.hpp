#pragma once
#include "domain/realm/character.hpp"
#include "domain/realm/dupe_checker.hpp"
#include "core/result.hpp"
#include <string>
#include <vector>
#include <span>

namespace pvpgn::application::realm {

struct SaveCharacterCommand {
    std::string account_name;
    std::string char_name;
    std::vector<uint8_t> save_data;
    bool check_dupes = true;
};

struct LoadCharacterCommand {
    std::string account_name;
    std::string char_name;
};

struct LoadCharacterResult {
    std::vector<uint8_t> save_data;
    std::string char_name;
    uint8_t level;
    uint8_t char_class;
    bool is_expansion;
    bool is_hardcore;
};

struct DeleteCharacterCommand {
    std::string account_name;
    std::string char_name;
};

class CharacterPersistenceUseCase {
public:
    explicit CharacterPersistenceUseCase(domain::realm::ISaveFileStore& store);
    
    core::Result<void, core::Error> save(const SaveCharacterCommand& cmd);
    core::Result<LoadCharacterResult, core::Error> load(const LoadCharacterCommand& cmd);
    core::Result<void, core::Error> remove(const DeleteCharacterCommand& cmd);
    core::Result<bool, core::Error> exists(std::string_view account, std::string_view char_name);
    
    // List all characters for an account
    core::Result<std::vector<std::string>, core::Error> 
        list_characters(std::string_view account_name);

private:
    domain::realm::ISaveFileStore& store_;
};

} // namespace pvpgn::application::realm
