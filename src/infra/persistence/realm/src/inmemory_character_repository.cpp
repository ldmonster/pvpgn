#include "infra/persistence/realm/inmemory_character_repository.hpp"

namespace pvpgn::infra::persistence::realm {

core::Result<domain::realm::Character, core::Error> 
InMemoryCharacterRepository::find(const domain::realm::CharacterId& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string key = make_key(id);
    auto it = characters_.find(key);
    
    if (it == characters_.end()) {
        return core::fail(
            core::make_error(core::StatusCode::NotFound, 
                           "Character not found: " + id.account_name + "/" + id.char_name)
        );
    }
    
    return core::Result<domain::realm::Character, core::Error>(it->second);
}

core::Result<void, core::Error> 
InMemoryCharacterRepository::save(const domain::realm::Character& character) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string key = make_key(character.id());
    characters_[key] = character;
    
    return core::Result<void, core::Error>();
}

core::Result<std::vector<domain::realm::Character>, core::Error>
InMemoryCharacterRepository::list_for_account(std::string_view account_name) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<domain::realm::Character> result;
    std::string prefix = std::string(account_name) + "/";
    
    for (const auto& [key, character] : characters_) {
        if (key.find(prefix) == 0) {
            result.push_back(character);
        }
    }
    
    return core::Result<std::vector<domain::realm::Character>, core::Error>(result);
}

} // namespace pvpgn::infra::persistence::realm
