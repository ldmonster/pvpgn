#pragma once
#include "core/result.hpp"
#include <string>
#include <vector>
#include <functional>
#include <cstdint>
#include <span>

namespace pvpgn::domain::realm {

enum class DupeCheckResult {
    clean,          // No dupes detected
    suspected,      // Possible dupe (hash mismatch)
    confirmed_dupe  // Definite dupe (same item in multiple saves)
};

struct DupeCheckReport {
    DupeCheckResult result;
    std::string char_name;
    std::string account_name;
    std::string reason;
    std::vector<std::string> suspected_items;
};

// Port for accessing save file storage
class ISaveFileStore {
public:
    virtual ~ISaveFileStore() = default;
    virtual core::Result<std::vector<uint8_t>, core::Error> 
        load(std::string_view account, std::string_view char_name) = 0;
    virtual core::Result<void, core::Error> 
        store(std::string_view account, std::string_view char_name, 
              std::span<const uint8_t> data) = 0;
    virtual core::Result<bool, core::Error> 
        exists(std::string_view account, std::string_view char_name) = 0;
    virtual core::Result<void, core::Error> 
        remove(std::string_view account, std::string_view char_name) = 0;
};

class DupeChecker {
public:
    // Check if a save file appears to be a dupe
    // Compares item GUIDs across all characters for the account
    static DupeCheckReport check(
        std::string_view account_name,
        std::string_view char_name,
        std::span<const uint8_t> save_data,
        const std::vector<std::pair<std::string, std::vector<uint8_t>>>& other_saves
    );
    
    // Quick hash-based check (fast, less accurate)
    static uint32_t compute_item_hash(std::span<const uint8_t> save_data);
    
    // Extract item GUIDs from save data (for thorough check)
    static std::vector<uint32_t> extract_item_guids(std::span<const uint8_t> save_data);
};

} // namespace pvpgn::domain::realm
