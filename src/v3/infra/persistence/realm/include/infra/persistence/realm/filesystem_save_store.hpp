#pragma once
#include "domain/realm/dupe_checker.hpp"
#include <filesystem>

namespace pvpgn::infra::persistence::realm {

class FilesystemSaveStore : public domain::realm::ISaveFileStore {
public:
    explicit FilesystemSaveStore(std::filesystem::path base_dir);
    
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
    std::filesystem::path base_dir_;
    std::filesystem::path make_path(std::string_view account, std::string_view char_name) const;
};

} // namespace pvpgn::infra::persistence::realm
