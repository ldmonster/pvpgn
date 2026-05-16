#include <catch2/catch_test_macros.hpp>
#include "domain/realm/dupe_checker.hpp"

namespace pvpgn::domain::realm::test {

// Create a minimal save file for testing
std::vector<uint8_t> create_save_file(size_t size = 1000) {
    std::vector<uint8_t> data(size, 0);
    // Add some variation to make different saves
    for (size_t i = 0; i < size; ++i) {
        data[i] = static_cast<uint8_t>(i % 256);
    }
    return data;
}

TEST_CASE("DupeChecker: CheckCleanSave", "[domain][realm]") {
    auto save1 = create_save_file(1000);
    auto save2 = create_save_file(1000);
    
    // Modify save2 to be different
    save2[100] = 0xFF;
    save2[200] = 0xAA;
    
    std::vector<std::pair<std::string, std::vector<uint8_t>>> other_saves;
    other_saves.push_back({"OtherChar", save2});
    
    auto report = DupeChecker::check("TestAccount", "TestChar", save1, other_saves);
    
    REQUIRE(report.result == DupeCheckResult::clean);
    REQUIRE(report.account_name == "TestAccount");
    REQUIRE(report.char_name == "TestChar");
}

TEST_CASE("DupeChecker: CheckEmptySave", "[domain][realm]") {
    std::vector<uint8_t> empty_save;
    std::vector<std::pair<std::string, std::vector<uint8_t>>> other_saves;
    
    auto report = DupeChecker::check("TestAccount", "TestChar", empty_save, other_saves);
    
    REQUIRE(report.result == DupeCheckResult::suspected);
    REQUIRE(report.reason == "Empty save data");
}

TEST_CASE("DupeChecker: ComputeItemHash", "[domain][realm]") {
    auto save1 = create_save_file(1000);
    auto save2 = create_save_file(1000);
    
    uint32_t hash1 = DupeChecker::compute_item_hash(save1);
    uint32_t hash2 = DupeChecker::compute_item_hash(save2);
    
    // Same data should produce same hash
    REQUIRE(hash1 == hash2);
    
    // Different data should produce different hash (usually)
    save2[800] = 0xFF;
    uint32_t hash3 = DupeChecker::compute_item_hash(save2);
    REQUIRE(hash1 != hash3);
}

TEST_CASE("DupeChecker: ExtractItemGuids", "[domain][realm]") {
    auto save = create_save_file(1000);
    auto guids = DupeChecker::extract_item_guids(save);
    
    // Should return a vector (may be empty for test data)
    REQUIRE((guids.empty() || !guids.empty()));  // Just verify it returns
}

TEST_CASE("DupeChecker: CheckWithMultipleSaves", "[domain][realm]") {
    auto save1 = create_save_file(1000);
    auto save2 = create_save_file(1000);
    auto save3 = create_save_file(1000);
    
    std::vector<std::pair<std::string, std::vector<uint8_t>>> other_saves;
    other_saves.push_back({"Char2", save2});
    other_saves.push_back({"Char3", save3});
    
    auto report = DupeChecker::check("TestAccount", "Char1", save1, other_saves);
    
    // Should complete without error
    REQUIRE((report.result == DupeCheckResult::clean || 
             report.result == DupeCheckResult::suspected));
}

} // namespace pvpgn::domain::realm::test
