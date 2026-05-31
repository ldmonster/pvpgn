// SPDX-License-Identifier: GPL-2.0-or-later
//
// Unit tests for SaveCharacterUseCase.

#include <catch2/catch_test_macros.hpp>

#include "application/realm/save_character.hpp"
#include "core/result.hpp"
#include "domain/realm/character.hpp"
#include "domain/realm/dupe_checker.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace pvpgn::application::realm {

class FakeCharRepo5 final : public ICharacterRepository {
public:
    core::Result<domain::realm::Character, core::Error>
    find(const domain::realm::CharacterId& id) override {
        auto key = id.account_name + "/" + id.char_name;
        auto it  = chars_.find(key);
        if (it == chars_.end())
            return core::fail(core::make_error(core::StatusCode::NotFound, "not found"));
        return core::Result<domain::realm::Character, core::Error>(it->second);
    }

    core::Result<void, core::Error>
    save(const domain::realm::Character& c) override {
        chars_.insert_or_assign(c.id().account_name + "/" + c.id().char_name, c);
        return core::Result<void, core::Error>();
    }

    core::Result<void, core::Error>
    remove(const domain::realm::CharacterId& id) override {
        chars_.erase(id.account_name + "/" + id.char_name);
        return core::Result<void, core::Error>();
    }

    core::Result<std::vector<domain::realm::Character>, core::Error>
    list_for_account(std::string_view account_name) override {
        std::vector<domain::realm::Character> result;
        for (auto& [k, v] : chars_)
            if (v.id().account_name == account_name)
                result.push_back(v);
        return core::Result<std::vector<domain::realm::Character>, core::Error>(
            std::move(result));
    }

    void seed(domain::realm::Character c) {
        chars_.insert_or_assign(c.id().account_name + "/" + c.id().char_name, std::move(c));
    }

private:
    std::unordered_map<std::string, domain::realm::Character> chars_;
};

class FakeSaveStore5 final : public domain::realm::ISaveFileStore {
public:
    core::Result<std::vector<uint8_t>, core::Error>
    load(std::string_view account, std::string_view char_name) override {
        auto key = std::string{account} + "/" + std::string{char_name};
        auto it  = files_.find(key);
        if (it == files_.end())
            return core::fail(core::make_error(core::StatusCode::NotFound, "no save"));
        return core::Result<std::vector<uint8_t>, core::Error>(it->second);
    }

    core::Result<void, core::Error>
    store(std::string_view account, std::string_view char_name,
          std::span<const uint8_t> data) override {
        files_[std::string{account} + "/" + std::string{char_name}] =
            std::vector<uint8_t>(data.begin(), data.end());
        return core::Result<void, core::Error>();
    }

    core::Result<bool, core::Error>
    exists(std::string_view account, std::string_view char_name) override {
        return core::Result<bool, core::Error>(
            files_.count(std::string{account} + "/" + std::string{char_name}) > 0);
    }

    core::Result<void, core::Error>
    remove(std::string_view account, std::string_view char_name) override {
        files_.erase(std::string{account} + "/" + std::string{char_name});
        return core::Result<void, core::Error>();
    }

    std::vector<uint8_t> get(std::string_view account,
                              std::string_view char_name) const {
        auto it = files_.find(std::string{account} + "/" + std::string{char_name});
        return it == files_.end() ? std::vector<uint8_t>{} : it->second;
    }

private:
    std::unordered_map<std::string, std::vector<uint8_t>> files_;
};

}  // namespace pvpgn::application::realm

namespace pa = pvpgn::application::realm;
namespace dr = pvpgn::domain::realm;

static dr::Character make_char5(std::string account, std::string name,
                                  bool locked = false,
                                  std::string gs = "gs1.example.com") {
    dr::CharacterId id{std::move(account), std::move(name)};
    dr::CharacterStats stats;
    dr::Character c{std::move(id), stats, {}};
    if (locked)
        (void)c.lock(std::move(gs));
    return c;
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST_CASE("SaveCharacterUseCase: happy path stores save data",
          "[application][realm][save_character]") {
    pa::FakeCharRepo5  char_repo;
    pa::FakeSaveStore5 store;

    // Character must be locked (in a game) to accept a save
    char_repo.seed(make_char5("alice", "Sorc", /*locked=*/true, "gs1.example.com"));

    pa::SaveCharacterUseCase uc{char_repo, store};
    pa::SaveCharacterCommand cmd;
    cmd.account_name = "alice";
    cmd.char_name    = "Sorc";
    cmd.save_data    = {0xDE, 0xAD, 0xBE, 0xEF};
    cmd.gs_address   = "gs1.example.com";

    auto result = uc.execute(cmd);
    REQUIRE(result.has_value());
    CHECK(store.get("alice", "Sorc") == std::vector<uint8_t>{0xDE, 0xAD, 0xBE, 0xEF});
}

TEST_CASE("SaveCharacterUseCase: empty account_name returns InvalidArgument",
          "[application][realm][save_character]") {
    pa::FakeCharRepo5  char_repo;
    pa::FakeSaveStore5 store;
    pa::SaveCharacterUseCase uc{char_repo, store};

    pa::SaveCharacterCommand cmd;
    cmd.account_name = "";
    cmd.char_name    = "Sorc";
    cmd.save_data    = {0x01};
    cmd.gs_address   = "gs1";

    auto result = uc.execute(cmd);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == pvpgn::core::StatusCode::InvalidArgument);
}

TEST_CASE("SaveCharacterUseCase: empty save_data returns InvalidArgument",
          "[application][realm][save_character]") {
    pa::FakeCharRepo5  char_repo;
    pa::FakeSaveStore5 store;
    pa::SaveCharacterUseCase uc{char_repo, store};

    pa::SaveCharacterCommand cmd;
    cmd.account_name = "alice";
    cmd.char_name    = "Sorc";
    cmd.save_data    = {};  // empty!
    cmd.gs_address   = "gs1";

    auto result = uc.execute(cmd);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == pvpgn::core::StatusCode::InvalidArgument);
}

TEST_CASE("SaveCharacterUseCase: non-existent character returns NotFound",
          "[application][realm][save_character]") {
    pa::FakeCharRepo5  char_repo;
    pa::FakeSaveStore5 store;
    pa::SaveCharacterUseCase uc{char_repo, store};

    pa::SaveCharacterCommand cmd;
    cmd.account_name = "alice";
    cmd.char_name    = "Ghost";
    cmd.save_data    = {0x01};
    cmd.gs_address   = "gs1";

    auto result = uc.execute(cmd);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == pvpgn::core::StatusCode::NotFound);
}

TEST_CASE("SaveCharacterUseCase: unlocked character returns FailedPrecondition",
          "[application][realm][save_character]") {
    pa::FakeCharRepo5  char_repo;
    pa::FakeSaveStore5 store;

    // Character is NOT locked — save should be rejected
    char_repo.seed(make_char5("alice", "Pala", /*locked=*/false));

    pa::SaveCharacterUseCase uc{char_repo, store};
    pa::SaveCharacterCommand cmd;
    cmd.account_name = "alice";
    cmd.char_name    = "Pala";
    cmd.save_data    = {0x01};
    cmd.gs_address   = "gs1.example.com";

    auto result = uc.execute(cmd);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == pvpgn::core::StatusCode::FailedPrecondition);
}
