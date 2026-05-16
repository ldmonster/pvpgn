#include "infra/persistence/realm/inmemory_save_store.hpp"

namespace pvpgn::infra::persistence::realm {

std::string InMemorySaveStore::make_key(std::string_view account, std::string_view char_name) const {
    return std::string(account) + ":" + std::string(char_name);
}

core::Result<std::vector<uint8_t>, core::Error> 
InMemorySaveStore::load(std::string_view account, std::string_view char_name) {
    auto account_it = storage_.find(std::string(account));
    if (account_it == storage_.end()) {
        return core::fail(core::Error(
            core::StatusCode::NotFound,
            "Account not found"
        ));
    }
    
    auto char_it = account_it->second.find(std::string(char_name));
    if (char_it == account_it->second.end()) {
        return core::fail(core::Error(
            core::StatusCode::NotFound,
            "Character not found"
        ));
    }
    
    return core::Result<std::vector<uint8_t>, core::Error>(char_it->second);
}

core::Result<void, core::Error> 
InMemorySaveStore::store(std::string_view account, std::string_view char_name, 
                         std::span<const uint8_t> data) {
    auto& account_map = storage_[std::string(account)];
    account_map[std::string(char_name)] = std::vector<uint8_t>(data.begin(), data.end());
    return core::Result<void, core::Error>();
}

core::Result<bool, core::Error> 
InMemorySaveStore::exists(std::string_view account, std::string_view char_name) {
    auto account_it = storage_.find(std::string(account));
    if (account_it == storage_.end()) {
        return core::Result<bool, core::Error>(false);
    }
    
    auto char_it = account_it->second.find(std::string(char_name));
    return core::Result<bool, core::Error>(char_it != account_it->second.end());
}

core::Result<void, core::Error> 
InMemorySaveStore::remove(std::string_view account, std::string_view char_name) {
    auto account_it = storage_.find(std::string(account));
    if (account_it == storage_.end()) {
        return core::fail(core::Error(
            core::StatusCode::NotFound,
            "Account not found"
        ));
    }
    
    auto char_it = account_it->second.find(std::string(char_name));
    if (char_it == account_it->second.end()) {
        return core::fail(core::Error(
            core::StatusCode::NotFound,
            "Character not found"
        ));
    }
    
    account_it->second.erase(char_it);
    return core::Result<void, core::Error>();
}

} // namespace pvpgn::infra::persistence::realm
