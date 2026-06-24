// SPDX-License-Identifier: GPL-2.0-or-later
//
// Unit tests targeting the parse-failure and store error paths of
// CharacterPersistenceUseCase (character_persistence.cpp). Complements the
// happy-path coverage in character_persistence_test.cpp.

#include <catch2/catch_test_macros.hpp>
#include "application/realm/character_persistence.hpp"
#include "domain/realm/character.hpp"
#include "core/result.hpp"
#include <map>
#include <vector>

namespace pvpgn::application::realm::test {

// A valid D2 save blob: signature 0xAA55AA55 (little-endian), version 96,
// padded to MIN_FILE_SIZE (335). Mirrors make_valid_save() in
// character_persistence_test.cpp.
inline std::vector<uint8_t> make_valid_save() {
    std::vector<uint8_t> data(335, 0);
    data[0] = 0x55; data[1] = 0xAA; data[2] = 0x55; data[3] = 0xAA;  // signature
    data[4] = 96;                                                    // version 110
    return data;
}

// Save store that records the requested operation but can be configured to fail
// store()/load() so the error-propagation branches in the use case are exercised.
class FailingSaveFileStore : public domain::realm::ISaveFileStore {
public:
    bool fail_store = false;
    bool fail_load  = false;
    std::map<std::string, std::vector<uint8_t>> storage;

    core::Result<std::vector<uint8_t>, core::Error>
    load(std::string_view account, std::string_view char_name) override {
        if (fail_load) {
            return core::fail(core::make_error(core::StatusCode::Internal, "load boom"));
        }
        std::string key = std::string(account) + ":" + std::string(char_name);
        auto it = storage.find(key);
        if (it == storage.end()) {
            return core::fail(core::make_error(core::StatusCode::NotFound, "not found"));
        }
        return core::Result<std::vector<uint8_t>, core::Error>(it->second);
    }

    core::Result<void, core::Error>
    store(std::string_view account, std::string_view char_name,
          std::span<const uint8_t> data) override {
        if (fail_store) {
            return core::fail(core::make_error(core::StatusCode::Internal, "store boom"));
        }
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

TEST_CASE("CharacterPersistence: save rejects empty save_data", "[application][realm]") {
    FailingSaveFileStore store;
    CharacterPersistenceUseCase use_case(store);

    SaveCharacterCommand cmd;
    cmd.account_name = "acct";
    cmd.char_name    = "hero";
    cmd.save_data    = {};  // empty

    auto result = use_case.save(cmd);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == core::StatusCode::InvalidArgument);
    // Nothing should have been written.
    CHECK(store.storage.empty());
}

TEST_CASE("CharacterPersistence: save rejects too-short blob (parse failure)",
          "[application][realm]") {
    FailingSaveFileStore store;
    CharacterPersistenceUseCase use_case(store);

    SaveCharacterCommand cmd;
    cmd.account_name = "acct";
    cmd.char_name    = "hero";
    cmd.save_data    = std::vector<uint8_t>(100, 0);  // < MIN_FILE_SIZE (335)

    auto result = use_case.save(cmd);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == core::StatusCode::InvalidArgument);
    CHECK(store.storage.empty());
}

TEST_CASE("CharacterPersistence: save rejects bad signature (parse failure)",
          "[application][realm]") {
    FailingSaveFileStore store;
    CharacterPersistenceUseCase use_case(store);

    auto blob = make_valid_save();
    blob[0] = 0x00;  // corrupt the signature, size still valid

    SaveCharacterCommand cmd;
    cmd.account_name = "acct";
    cmd.char_name    = "hero";
    cmd.save_data    = blob;

    auto result = use_case.save(cmd);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == core::StatusCode::InvalidArgument);
}

TEST_CASE("CharacterPersistence: save rejects unsupported version (parse failure)",
          "[application][realm]") {
    FailingSaveFileStore store;
    CharacterPersistenceUseCase use_case(store);

    auto blob = make_valid_save();
    blob[4] = 5;  // unsupported version (not 87 or 96)

    SaveCharacterCommand cmd;
    cmd.account_name = "acct";
    cmd.char_name    = "hero";
    cmd.save_data    = blob;

    auto result = use_case.save(cmd);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == core::StatusCode::InvalidArgument);
}

TEST_CASE("CharacterPersistence: save propagates store error", "[application][realm]") {
    FailingSaveFileStore store;
    store.fail_store = true;
    CharacterPersistenceUseCase use_case(store);

    SaveCharacterCommand cmd;
    cmd.account_name = "acct";
    cmd.char_name    = "hero";
    cmd.save_data    = make_valid_save();

    auto result = use_case.save(cmd);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == core::StatusCode::Internal);
}

TEST_CASE("CharacterPersistence: load empty account is InvalidArgument",
          "[application][realm]") {
    FailingSaveFileStore store;
    CharacterPersistenceUseCase use_case(store);

    LoadCharacterCommand cmd;
    cmd.account_name = "";
    cmd.char_name    = "hero";

    auto result = use_case.load(cmd);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == core::StatusCode::InvalidArgument);
}

TEST_CASE("CharacterPersistence: load empty char name is InvalidArgument",
          "[application][realm]") {
    FailingSaveFileStore store;
    CharacterPersistenceUseCase use_case(store);

    LoadCharacterCommand cmd;
    cmd.account_name = "acct";
    cmd.char_name    = "";

    auto result = use_case.load(cmd);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == core::StatusCode::InvalidArgument);
}

TEST_CASE("CharacterPersistence: load propagates store load error",
          "[application][realm]") {
    FailingSaveFileStore store;
    store.fail_load = true;
    CharacterPersistenceUseCase use_case(store);

    LoadCharacterCommand cmd;
    cmd.account_name = "acct";
    cmd.char_name    = "hero";

    auto result = use_case.load(cmd);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == core::StatusCode::Internal);
}

TEST_CASE("CharacterPersistence: load fails to parse corrupt stored blob",
          "[application][realm]") {
    FailingSaveFileStore store;
    // Stash a too-short blob directly, bypassing save()'s validation.
    store.storage["acct:hero"] = std::vector<uint8_t>(10, 0);
    CharacterPersistenceUseCase use_case(store);

    LoadCharacterCommand cmd;
    cmd.account_name = "acct";
    cmd.char_name    = "hero";

    auto result = use_case.load(cmd);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == core::StatusCode::InvalidArgument);
}

TEST_CASE("CharacterPersistence: load returns metadata for a valid blob",
          "[application][realm]") {
    FailingSaveFileStore store;
    CharacterPersistenceUseCase use_case(store);

    // Round-trip a valid save then load it, checking extracted metadata.
    // Canonical D2S v96 layout: status byte at offset 36 (hardcore 0x04,
    // expansion 0x20), level at offset 43.
    auto blob = make_valid_save();
    blob[36] = 0x04 | 0x20;  // char_status: hardcore + expansion
    blob[43] = 42;           // level

    SaveCharacterCommand save_cmd;
    save_cmd.account_name = "acct";
    save_cmd.char_name    = "hero";
    save_cmd.save_data    = blob;
    REQUIRE(use_case.save(save_cmd).has_value());

    LoadCharacterCommand load_cmd;
    load_cmd.account_name = "acct";
    load_cmd.char_name    = "hero";

    auto result = use_case.load(load_cmd);
    REQUIRE(result.has_value());
    auto loaded = result.value();
    CHECK(loaded.char_name == "hero");
    CHECK(loaded.level == 42);
    CHECK(loaded.is_hardcore == true);
    CHECK(loaded.is_expansion == true);
}

TEST_CASE("CharacterPersistence: remove empty account is InvalidArgument",
          "[application][realm]") {
    FailingSaveFileStore store;
    CharacterPersistenceUseCase use_case(store);

    DeleteCharacterCommand cmd;
    cmd.account_name = "";
    cmd.char_name    = "hero";

    auto result = use_case.remove(cmd);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == core::StatusCode::InvalidArgument);
}

TEST_CASE("CharacterPersistence: remove empty char name is InvalidArgument",
          "[application][realm]") {
    FailingSaveFileStore store;
    CharacterPersistenceUseCase use_case(store);

    DeleteCharacterCommand cmd;
    cmd.account_name = "acct";
    cmd.char_name    = "";

    auto result = use_case.remove(cmd);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == core::StatusCode::InvalidArgument);
}

TEST_CASE("CharacterPersistence: exists empty inputs are InvalidArgument",
          "[application][realm]") {
    FailingSaveFileStore store;
    CharacterPersistenceUseCase use_case(store);

    auto r1 = use_case.exists("", "hero");
    REQUIRE_FALSE(r1.has_value());
    CHECK(r1.error().code() == core::StatusCode::InvalidArgument);

    auto r2 = use_case.exists("acct", "");
    REQUIRE_FALSE(r2.has_value());
    CHECK(r2.error().code() == core::StatusCode::InvalidArgument);
}

TEST_CASE("CharacterPersistence: list_characters empty account is InvalidArgument",
          "[application][realm]") {
    FailingSaveFileStore store;
    CharacterPersistenceUseCase use_case(store);

    auto result = use_case.list_characters("");
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == core::StatusCode::InvalidArgument);
}

} // namespace pvpgn::application::realm::test
