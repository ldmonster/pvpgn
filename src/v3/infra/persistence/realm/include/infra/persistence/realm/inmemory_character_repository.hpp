#pragma once
#include "application/realm/character_lock.hpp"
#include <unordered_map>
#include <mutex>

namespace pvpgn::infra::persistence::realm {

class InMemoryCharacterRepository : public application::realm::ICharacterRepository {
public:
    core::Result<domain::realm::Character, core::Error> 
        find(const domain::realm::CharacterId& id) override;
    core::Result<void, core::Error> 
        save(const domain::realm::Character& character) override;
    core::Result<std::vector<domain::realm::Character>, core::Error>
        list_for_account(std::string_view account_name) override;

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, domain::realm::Character> characters_;
    
    static std::string make_key(const domain::realm::CharacterId& id) {
        return id.account_name + "/" + id.char_name;
    }
};

} // namespace pvpgn::infra::persistence::realm
