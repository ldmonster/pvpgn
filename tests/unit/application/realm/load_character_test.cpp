// SPDX-License-Identifier: GPL-2.0-or-later
//
// Unit tests for LoadCharacterUseCase.

#include <catch2/catch_test_macros.hpp>

#include "application/realm/load_character.hpp"
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

// ---------------------------------------------------------------------------
// Inline fakes
// ---------------------------------------------------------------------------

class FakeCharRepo4 final : public ICharacterRepository {
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

class FakeSaveFileStore final : public domain::realm::ISaveFileStore {
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
        auto key = std::string{account} + "/" + std::string{char_name};
        files_[key] = std::vector<uint8_t>(data.begin(), data.end());
        return core::Result<void, core::Error>();
    }

    core::Result<bool, core::Error>
    exists(std::string_view account, std::string_view char_name) override {
        auto key = std::string{account} + "/" + std::string{char_name};
        return core::Result<bool, core::Error>(files_.count(key) > 0);
    }

    core::Result<void, core::Error>
    remove(std::string_view account, std::string_view char_name) override {
        files_.erase(std::string{account} + "/" + std::string{char_name});
        return core::Result<void, core::Error>();
    }

    void seed(std::string_view account, std::string_view char_name,
              std::vector<uint8_t> data) {
        files_[std::string{account} + "/" + std::string{char_name}] = std::move(data);
    }

private:
    std::unordered_map<std::string, std::vector<uint8_t>> files_;
};

}  // namespace pvpgn::application::realm

namespace pa = pvpgn::application::realm;
namespace dr = pvpgn::domain::realm;

static dr::Character make_char4(std::string account, std::string name,
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

TEST_CASE("LoadCharacterUseCase: happy path loads save data",
          "[application][realm][load_character]") {
    pa::FakeCharRepo4     char_repo;
    pa::FakeSaveFileStore store;

    char_repo.seed(make_char4("alice", "Amazon"));
    store.seed("alice", "Amazon", {0x01, 0x02, 0x03});

    pa::LoadCharacterUseCase uc{char_repo, store};
    pa::LoadCharacterCommand cmd{"alice", "Amazon", "gs1.example.com"};

    auto result = uc.execute(cmd);
    REQUIRE(result.has_value());
    CHECK(result.value().save_data == std::vector<uint8_t>{0x01, 0x02, 0x03});
}

TEST_CASE("LoadCharacterUseCase: empty account_name returns InvalidArgument",
          "[application][realm][load_character]") {
    pa::FakeCharRepo4     char_repo;
    pa::FakeSaveFileStore store;
    pa::LoadCharacterUseCase uc{char_repo, store};

    auto result = uc.execute({"", "Amazon", "gs1"});
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == pvpgn::core::StatusCode::InvalidArgument);
}

TEST_CASE("LoadCharacterUseCase: non-existent character returns NotFound",
          "[application][realm][load_character]") {
    pa::FakeCharRepo4     char_repo;
    pa::FakeSaveFileStore store;
    pa::LoadCharacterUseCase uc{char_repo, store};

    auto result = uc.execute({"alice", "Ghost", "gs1"});
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == pvpgn::core::StatusCode::NotFound);
}

TEST_CASE("LoadCharacterUseCase: character locked by different server returns FailedPrecondition",
          "[application][realm][load_character]") {
    pa::FakeCharRepo4     char_repo;
    pa::FakeSaveFileStore store;

    // Locked by gs2, but gs1 is requesting
    char_repo.seed(make_char4("alice", "Barb", /*locked=*/true, "gs2.example.com"));
    store.seed("alice", "Barb", {0xAA});

    pa::LoadCharacterUseCase uc{char_repo, store};
    auto result = uc.execute({"alice", "Barb", "gs1.example.com"});
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == pvpgn::core::StatusCode::FailedPrecondition);
}

TEST_CASE("LoadCharacterUseCase: character locked by same server is allowed",
          "[application][realm][load_character]") {
    pa::FakeCharRepo4     char_repo;
    pa::FakeSaveFileStore store;

    // Locked by gs1, and gs1 is requesting — should succeed
    char_repo.seed(make_char4("alice", "Druid", /*locked=*/true, "gs1.example.com"));
    store.seed("alice", "Druid", {0xBB, 0xCC});

    pa::LoadCharacterUseCase uc{char_repo, store};
    auto result = uc.execute({"alice", "Druid", "gs1.example.com"});
    REQUIRE(result.has_value());
    CHECK(result.value().save_data == std::vector<uint8_t>{0xBB, 0xCC});
}
