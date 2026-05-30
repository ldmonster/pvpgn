// SPDX-License-Identifier: GPL-2.0-or-later
//
// Unit tests for DeleteCharacterUseCase.

#include <catch2/catch_test_macros.hpp>

#include "application/realm/delete_character.hpp"
#include "core/result.hpp"
#include "domain/realm/character.hpp"
#include "domain/realm/character_list.hpp"

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace pvpgn::application::realm {

// ---------------------------------------------------------------------------
// Inline fakes (same pattern as create_character_test)
// ---------------------------------------------------------------------------

class FakeCharRepo2 final : public ICharacterRepository {
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
        chars_.emplace(c.id().account_name + "/" + c.id().char_name, c);
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
        chars_.emplace(c.id().account_name + "/" + c.id().char_name, std::move(c));
    }

    bool has(std::string_view account, std::string_view name) const {
        return chars_.count(std::string{account} + "/" + std::string{name}) > 0;
    }

private:
    std::unordered_map<std::string, domain::realm::Character> chars_;
};

class FakeListRepo2 final : public ICharacterListRepository {
public:
    core::Result<domain::realm::CharacterList, core::Error>
    find_for_account(std::string_view account_name) override {
        domain::realm::CharacterList list{std::string{account_name}};
        auto it = lists_.find(std::string{account_name});
        if (it != lists_.end())
            for (auto& c : it->second)
                (void)list.add(c);
        return core::Result<domain::realm::CharacterList, core::Error>(std::move(list));
    }

    core::Result<void, core::Error>
    save(std::string_view account_name,
         const domain::realm::CharacterList& list) override {
        auto& stored = lists_[std::string{account_name}];
        stored.clear();
        for (auto* cp : list.list(domain::realm::SortMode::by_name))
            stored.push_back(*cp);
        return core::Result<void, core::Error>();
    }

    void seed(std::string_view account_name, domain::realm::Character c) {
        lists_[std::string{account_name}].push_back(std::move(c));
    }

private:
    std::unordered_map<std::string,
                       std::vector<domain::realm::Character>> lists_;
};

}  // namespace pvpgn::application::realm

namespace pa  = pvpgn::application::realm;
namespace dr  = pvpgn::domain::realm;

static dr::Character make_char(std::string account, std::string name,
                                bool locked = false) {
    dr::CharacterId id{std::move(account), std::move(name)};
    dr::CharacterStats stats;
    dr::Character c{std::move(id), stats, {}};
    if (locked)
        (void)c.lock("gs1.example.com");
    return c;
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST_CASE("DeleteCharacterUseCase: happy path removes character",
          "[application][realm][delete_character]") {
    pa::FakeCharRepo2  char_repo;
    pa::FakeListRepo2  list_repo;

    char_repo.seed(make_char("alice", "Amazon"));
    list_repo.seed("alice", make_char("alice", "Amazon"));

    pa::DeleteCharacterUseCase uc{char_repo, list_repo};
    pa::DeleteCharacterCommand cmd{"alice", "Amazon"};

    auto result = uc.execute(cmd);
    REQUIRE(result.has_value());
    CHECK_FALSE(char_repo.has("alice", "Amazon"));
}

TEST_CASE("DeleteCharacterUseCase: empty account_name returns InvalidArgument",
          "[application][realm][delete_character]") {
    pa::FakeCharRepo2  char_repo;
    pa::FakeListRepo2  list_repo;
    pa::DeleteCharacterUseCase uc{char_repo, list_repo};

    auto result = uc.execute({"", "Amazon"});
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == pvpgn::core::StatusCode::InvalidArgument);
}

TEST_CASE("DeleteCharacterUseCase: empty char_name returns InvalidArgument",
          "[application][realm][delete_character]") {
    pa::FakeCharRepo2  char_repo;
    pa::FakeListRepo2  list_repo;
    pa::DeleteCharacterUseCase uc{char_repo, list_repo};

    auto result = uc.execute({"alice", ""});
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == pvpgn::core::StatusCode::InvalidArgument);
}

TEST_CASE("DeleteCharacterUseCase: non-existent character returns NotFound",
          "[application][realm][delete_character]") {
    pa::FakeCharRepo2  char_repo;
    pa::FakeListRepo2  list_repo;
    pa::DeleteCharacterUseCase uc{char_repo, list_repo};

    auto result = uc.execute({"alice", "Ghost"});
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == pvpgn::core::StatusCode::NotFound);
}

TEST_CASE("DeleteCharacterUseCase: locked character returns FailedPrecondition",
          "[application][realm][delete_character]") {
    pa::FakeCharRepo2  char_repo;
    pa::FakeListRepo2  list_repo;

    char_repo.seed(make_char("alice", "Necro", /*locked=*/true));
    list_repo.seed("alice", make_char("alice", "Necro", /*locked=*/true));

    pa::DeleteCharacterUseCase uc{char_repo, list_repo};
    auto result = uc.execute({"alice", "Necro"});
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == pvpgn::core::StatusCode::FailedPrecondition);
}
