#include <catch2/catch_test_macros.hpp>
#include "application/realm/character_persistence.hpp"
#include "domain/realm/character.hpp"
#include "core/result.hpp"
#include <memory>
#include <map>

namespace pvpgn::application::realm::test {

// Mock save file store for testing
class MockSaveFileStore : public domain::realm::ISaveFileStore {
public:
    std::map<std::string, std::vector<uint8_t>> storage;
    
    core::Result<std::vector<uint8_t>, core::Error>
    load(std::string_view account, std::string_view char_name) override {
        std::string key = std::string(account) + ":" + std::string(char_name);
        auto it = storage.find(key);
        if (it == storage.end()) {
            return core::fail(
                core::make_error(core::StatusCode::NotFound, "Character not found")
            );
        }
        return core::Result<std::vector<uint8_t>, core::Error>(it->second);
    }
    
    core::Result<void, core::Error>
    store(std::string_view account, std::string_view char_name, 
         std::span<const uint8_t> data) override {
        std::string key = std::string(account) + ":" + std::string(char_name);
        storage[key] = std::vector<uint8_t>(data.begin(), data.end());
        return core::Result<void, core::Error>();
    }
    
    core::Result<void, core::Error>
    remove(std::string_view account, std::string_view char_name) override {
        std::string key = std::string(account) + ":" + std::string(char_name);
        storage.erase(key);
        return core::Result<void, core::Error>();
    }
    
    core::Result<bool, core::Error>
    exists(std::string_view account, std::string_view char_name) override {
        std::string key = std::string(account) + ":" + std::string(char_name);
        return core::Result<bool, core::Error>(storage.find(key) != storage.end());
    }
};

// A minimal D2 save blob that satisfies D2SaveCodec::parse: valid signature
// (0xAA55AA55) + version (110) padded to MIN_FILE_SIZE. The use-case validates
// save data through the codec, so tests expecting save()/load() to succeed need
// a real blob rather than zero-filled bytes.
inline std::vector<uint8_t> make_valid_save() {
    std::vector<uint8_t> data(335, 0);
    data[0] = 0x55; data[1] = 0xAA; data[2] = 0x55; data[3] = 0xAA;  // signature
    data[4] = 96;                                                    // version 110
    return data;
}

TEST_CASE("CharacterPersistence: SaveCharacter") {
    MockSaveFileStore store;
    CharacterPersistenceUseCase use_case(store);

    SaveCharacterCommand cmd;
    cmd.account_name = "TestAccount";
    cmd.char_name = "TestChar";
    cmd.save_data = make_valid_save();
    cmd.check_dupes = false;

    auto result = use_case.save(cmd);
    REQUIRE(result.has_value());
}

TEST_CASE("CharacterPersistence: SaveCharacterEmptyAccount") {
    MockSaveFileStore store;
    CharacterPersistenceUseCase use_case(store);
    
    SaveCharacterCommand cmd;
    cmd.account_name = "";
    cmd.char_name = "TestChar";
    cmd.save_data = std::vector<uint8_t>(100, 0);
    
    auto result = use_case.save(cmd);
    REQUIRE(!result.has_value());
}

TEST_CASE("CharacterPersistence: SaveCharacterEmptyName") {
    MockSaveFileStore store;
    CharacterPersistenceUseCase use_case(store);
    
    SaveCharacterCommand cmd;
    cmd.account_name = "TestAccount";
    cmd.char_name = "";
    cmd.save_data = std::vector<uint8_t>(100, 0);
    
    auto result = use_case.save(cmd);
    REQUIRE(!result.has_value());
}

TEST_CASE("CharacterPersistence: SaveAndLoadCharacter") {
    MockSaveFileStore store;
    CharacterPersistenceUseCase use_case(store);
    
    SaveCharacterCommand save_cmd;
    save_cmd.account_name = "TestAccount";
    save_cmd.char_name = "TestChar";
    save_cmd.save_data = make_valid_save();
    save_cmd.check_dupes = false;

    auto save_result = use_case.save(save_cmd);
    REQUIRE(save_result.has_value());
    
    LoadCharacterCommand load_cmd;
    load_cmd.account_name = "TestAccount";
    load_cmd.char_name = "TestChar";
    
    auto load_result = use_case.load(load_cmd);
    REQUIRE(load_result.has_value());
    
    auto loaded = load_result.value();
    CHECK(loaded.char_name == "TestChar");
}

TEST_CASE("CharacterPersistence: CharacterExists") {
    MockSaveFileStore store;
    CharacterPersistenceUseCase use_case(store);
    
    SaveCharacterCommand cmd;
    cmd.account_name = "TestAccount";
    cmd.char_name = "TestChar";
    cmd.save_data = make_valid_save();
    cmd.check_dupes = false;

    auto save_result = use_case.save(cmd);
    REQUIRE(save_result.has_value());

    auto exists_result = use_case.exists("TestAccount", "TestChar");
    REQUIRE(exists_result.has_value());
    CHECK(exists_result.value());
}

TEST_CASE("CharacterPersistence: CharacterNotExists") {
    MockSaveFileStore store;
    CharacterPersistenceUseCase use_case(store);
    
    auto exists_result = use_case.exists("TestAccount", "NonExistent");
    REQUIRE(exists_result.has_value());
    CHECK(!exists_result.value());
}

TEST_CASE("CharacterPersistence: DeleteCharacter") {
    MockSaveFileStore store;
    CharacterPersistenceUseCase use_case(store);
    
    SaveCharacterCommand save_cmd;
    save_cmd.account_name = "TestAccount";
    save_cmd.char_name = "TestChar";
    save_cmd.save_data = std::vector<uint8_t>(100, 0);
    save_cmd.check_dupes = false;
    
    [[maybe_unused]] auto save_result = use_case.save(save_cmd);
    
    DeleteCharacterCommand del_cmd;
    del_cmd.account_name = "TestAccount";
    del_cmd.char_name = "TestChar";
    
    auto del_result = use_case.remove(del_cmd);
    CHECK(del_result.has_value());
    
    auto exists_result = use_case.exists("TestAccount", "TestChar");
    REQUIRE(exists_result.has_value());
    CHECK(!exists_result.value());
}

TEST_CASE("CharacterPersistence: ListCharacters") {
    MockSaveFileStore store;
    CharacterPersistenceUseCase use_case(store);
    
    auto list_result = use_case.list_characters("TestAccount");
    CHECK(list_result.has_value());
}

} // namespace pvpgn::application::realm::test
