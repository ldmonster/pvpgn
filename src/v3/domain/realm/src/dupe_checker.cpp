#include "domain/realm/dupe_checker.hpp"
#include <algorithm>
#include <cstring>

namespace pvpgn::domain::realm {

namespace {

// D2 save file item section starts after header (around offset 767)
// Items are stored in a specific binary format with GUIDs
constexpr size_t D2_ITEM_SECTION_OFFSET = 767;

// D2 save files have a minimum size (header + some data)
constexpr size_t D2_MIN_SAVE_SIZE = 1000;

// D2 save files start with specific magic bytes
constexpr uint32_t D2_SAVE_MAGIC = 0x55AAAA55;

// Simple FNV-1a hash for item data
uint32_t fnv1a_hash(std::span<const uint8_t> data) {
    uint32_t hash = 2166136261u;
    for (uint8_t byte : data) {
        hash ^= byte;
        hash *= 16777619u;
    }
    return hash;
}

// Check if save data looks like a valid D2 save file
bool is_valid_d2_save(std::span<const uint8_t> data) {
    if (data.size() < D2_MIN_SAVE_SIZE) {
        return false;
    }
    
    // Check for D2 save magic bytes at the start
    if (data.size() >= 4) {
        uint32_t magic;
        std::memcpy(&magic, data.data(), sizeof(uint32_t));
        if (magic == D2_SAVE_MAGIC) {
            return true;
        }
    }
    
    return false;
}

} // namespace

DupeCheckReport DupeChecker::check(
    std::string_view account_name,
    std::string_view char_name,
    std::span<const uint8_t> save_data,
    const std::vector<std::pair<std::string, std::vector<uint8_t>>>& other_saves
) {
    DupeCheckReport report;
    report.account_name = std::string(account_name);
    report.char_name = std::string(char_name);
    report.result = DupeCheckResult::clean;

    if (save_data.empty()) {
        report.result = DupeCheckResult::suspected;
        report.reason = "Empty save data";
        return report;
    }

    // Only do GUID-based checking if this looks like a valid D2 save
    if (is_valid_d2_save(save_data)) {
        auto current_guids = extract_item_guids(save_data);
        
        // Check against other saves
        for (const auto& [other_char_name, other_data] : other_saves) {
            if (other_data.empty() || !is_valid_d2_save(other_data)) {
                continue;
            }

            auto other_guids = extract_item_guids(other_data);

            // Check for common GUIDs (unique items)
            for (uint32_t guid : current_guids) {
                if (guid == 0) continue;  // Skip invalid GUIDs
                
                auto it = std::find(other_guids.begin(), other_guids.end(), guid);
                if (it != other_guids.end()) {
                    report.result = DupeCheckResult::confirmed_dupe;
                    report.reason = "Duplicate item GUID found in character: " + other_char_name;
                    report.suspected_items.push_back(other_char_name);
                }
            }
        }
    }

    // If no confirmed dupes, do a hash-based check
    if (report.result == DupeCheckResult::clean) {
        uint32_t current_hash = compute_item_hash(save_data);
        
        for (const auto& [other_char_name, other_data] : other_saves) {
            if (other_data.empty()) {
                continue;
            }

            uint32_t other_hash = compute_item_hash(other_data);
            
            // If hashes match, it's suspicious (same items)
            // Only flag as suspected if hashes are identical AND non-zero
            // AND the saves are actually identical (not just hash collision)
            if (current_hash == other_hash && current_hash != 0) {
                // Double-check: compare actual data to avoid false positives
                if (save_data.size() == other_data.size() && 
                    std::equal(save_data.begin(), save_data.end(), other_data.begin())) {
                    report.result = DupeCheckResult::suspected;
                    report.reason = "Item hash collision with character: " + other_char_name;
                    report.suspected_items.push_back(other_char_name);
                }
            }
        }
    }

    return report;
}

uint32_t DupeChecker::compute_item_hash(std::span<const uint8_t> save_data) {
    if (save_data.size() <= D2_ITEM_SECTION_OFFSET) {
        return 0;
    }

    // Hash the item section of the save file
    auto item_section = save_data.subspan(D2_ITEM_SECTION_OFFSET);
    return fnv1a_hash(item_section);
}

std::vector<uint32_t> DupeChecker::extract_item_guids(std::span<const uint8_t> save_data) {
    std::vector<uint32_t> guids;

    if (save_data.size() <= D2_ITEM_SECTION_OFFSET) {
        return guids;
    }

    // D2 items have a specific structure with GUID at specific offsets
    // This is a simplified extraction - real implementation would parse item headers
    auto item_section = save_data.subspan(D2_ITEM_SECTION_OFFSET);

    // Look for item markers (0x4A = 'J' in D2 format)
    for (size_t i = 0; i + 4 < item_section.size(); ++i) {
        if (item_section[i] == 0x4A) {  // Item marker
            // GUID is typically 4 bytes after the marker
            if (i + 12 < item_section.size()) {
                uint32_t guid;
                std::memcpy(&guid, item_section.data() + i + 8, sizeof(uint32_t));
                if (guid != 0) {
                    guids.push_back(guid);
                }
            }
        }
    }

    return guids;
}

} // namespace pvpgn::domain::realm
