#include "infra/persistence/realm/filesystem_save_store.hpp"
#include <fstream>
#include <sstream>

namespace pvpgn::infra::persistence::realm {

FilesystemSaveStore::FilesystemSaveStore(std::filesystem::path base_dir)
    : base_dir_(std::move(base_dir)) {
    // Ensure base directory exists
    std::filesystem::create_directories(base_dir_);
}

std::filesystem::path FilesystemSaveStore::make_path(std::string_view account, std::string_view char_name) const {
    // Create account subdirectory
    auto account_dir = base_dir_ / std::string(account);
    return account_dir / (std::string(char_name) + ".d2s");
}

core::Result<std::vector<uint8_t>, core::Error> 
FilesystemSaveStore::load(std::string_view account, std::string_view char_name) {
    auto path = make_path(account, char_name);
    
    if (!std::filesystem::exists(path)) {
        return core::fail(core::Error(
            core::StatusCode::NotFound,
            "Save file not found: " + path.string()
        ));
    }
    
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return core::fail(core::Error(
            core::StatusCode::Internal,
            "Failed to open save file: " + path.string()
        ));
    }
    
    // Read entire file
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::vector<uint8_t> data(size);
    file.read(reinterpret_cast<char*>(data.data()), size);
    
    if (!file) {
        return core::fail(core::Error(
            core::StatusCode::Internal,
            "Failed to read save file: " + path.string()
        ));
    }
    
    return core::Result<std::vector<uint8_t>, core::Error>(data);
}

core::Result<void, core::Error> 
FilesystemSaveStore::store(std::string_view account, std::string_view char_name, 
                           std::span<const uint8_t> data) {
    auto path = make_path(account, char_name);
    
    // Create account directory if it doesn't exist
    auto account_dir = path.parent_path();
    std::error_code ec;
    std::filesystem::create_directories(account_dir, ec);
    if (ec) {
        return core::fail(core::Error(
            core::StatusCode::Internal,
            "Failed to create account directory: " + ec.message()
        ));
    }
    
    // Write save file
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return core::fail(core::Error(
            core::StatusCode::Internal,
            "Failed to open save file for writing: " + path.string()
        ));
    }
    
    file.write(reinterpret_cast<const char*>(data.data()), data.size());
    
    if (!file) {
        return core::fail(core::Error(
            core::StatusCode::Internal,
            "Failed to write save file: " + path.string()
        ));
    }
    
    return core::Result<void, core::Error>();
}

core::Result<bool, core::Error> 
FilesystemSaveStore::exists(std::string_view account, std::string_view char_name) {
    auto path = make_path(account, char_name);
    return core::Result<bool, core::Error>(std::filesystem::exists(path));
}

core::Result<void, core::Error> 
FilesystemSaveStore::remove(std::string_view account, std::string_view char_name) {
    auto path = make_path(account, char_name);
    
    if (!std::filesystem::exists(path)) {
        return core::fail(core::Error(
            core::StatusCode::NotFound,
            "Save file not found: " + path.string()
        ));
    }
    
    std::error_code ec;
    std::filesystem::remove(path, ec);
    if (ec) {
        return core::fail(core::Error(
            core::StatusCode::Internal,
            "Failed to remove save file: " + ec.message()
        ));
    }
    
    return core::Result<void, core::Error>();
}

} // namespace pvpgn::infra::persistence::realm
