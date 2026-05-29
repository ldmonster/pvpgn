#pragma once
#include "domain/realm/dupe_checker.hpp"
#include <map>
#include <string>

namespace pvpgn::infra::persistence::realm {

class InMemorySaveStore : public domain::realm::ISaveFileStore {
public:
    InMemorySaveStore() = default;
    
    core::Result<std::vector<uint8_t>, core::Error> 
        load(std::string_view account, std::string_view char_name) override;
    core::Result<void, core::Error> 
        store(std::string_view account, std::string_view char_name, 
              std::span<const uint8_t> data) override;
    core::Result<bool, core::Error> 
        exists(std::string_view account, std::string_view char_name) override;
    core::Result<void, core::Error> 
        remove(std::string_view account, std::string_view char_name) override;

private:
    // Map: account_name -> (char_name -> save_data)
    std::map<std::string, std::map<std::string, std::vector<uint8_t>>> storage_;
    
    std::string make_key(std::string_view account, std::string_view char_name) const;
};

} // namespace pvpgn::infra::persistence::realm
